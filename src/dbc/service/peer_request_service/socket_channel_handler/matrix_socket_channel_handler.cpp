#include "matrix_socket_channel_handler.h"
#include "service_proto_filter.h"
#include "network/compress/matrix_compress.h"
#include "service/message/message_id.h"
#include "network/topic_manager.h"

matrix_socket_channel_handler::matrix_socket_channel_handler(std::shared_ptr<network::channel> ch)
    : m_stopped(false)
    , m_coder(std::make_shared<matrix_coder>())
    , m_decoder(std::make_shared<matrix_coder>())
    , m_channel(ch)
    , m_shake_hand_timer(*(ch->get_io_service()))
    , m_has_message(false)
    , m_login_success(false)
    , m_sid(ch->id())
{
    if (ConfManager::MAX_RECV_SPEED > 0)
    {
        int32_t recv_speed = ConfManager::MAX_RECV_SPEED;
        int32_t cycle = 1;
        int32_t slice = 2;
        m_f_ctl = std::make_shared<network::flow_ctrl>(recv_speed, cycle, slice, ch->get_io_service());
        if (m_f_ctl != nullptr)
        {
            m_f_ctl->start();
        }

    }
}

matrix_socket_channel_handler::~matrix_socket_channel_handler()
{
    
}

int32_t matrix_socket_channel_handler::stop()
{
    m_stopped = true;
    stop_shake_hand_timer();
    if (ConfManager::MAX_RECV_SPEED > 0 && m_f_ctl != nullptr)
    {
        m_f_ctl->stop();
    }

    return ERR_SUCCESS;
}

bool matrix_socket_channel_handler::validate_resp_path(std::string msg_name, std::vector<std::string>& path)
{
    if (msg_name != NODE_LIST_TASK_RSP &&
        msg_name != NODE_TASK_LOGS_RSP &&
        msg_name != NODE_QUERY_NODE_INFO_RSP)
    {
        return true;
    }

    uint32_t path_len = path.size();
    if (path_len > 20)
    {
        LOG_DEBUG << "path length invalid " << path_len << std::endl;
        return false;
    }

    auto it = std::unique (path.begin(), path.end());
    path.resize( std::distance(path.begin(),it) );

    if(path_len != path.size())
    {
        LOG_DEBUG << "path has duplicate elements";
        return false;
    }

    return true;
}

bool matrix_socket_channel_handler::validate_req_path(std::string msg_name, std::vector<std::string>& path)
{
    if( msg_name != NODE_LIST_TASK_REQ &&
        msg_name != NODE_TASK_LOGS_REQ &&
        msg_name != NODE_QUERY_NODE_INFO_REQ)
    {
        return true;
    }

    uint32_t path_len=path.size();
    if (path_len == 0 || path_len > 100)
    {
        LOG_ERROR << "path length invalid "<< path_len << std::endl;
        return false;
    }

    auto ch_ = m_channel.lock();
    if( nullptr == ch_)
    {
        return false;
    }

    std::shared_ptr<network::tcp_socket_channel> ch = 
        std::dynamic_pointer_cast<network::tcp_socket_channel>(ch_);
    if (nullptr == ch)
    {
        return false;
    }

    std::string peer_id = ch->get_remote_node_id();
    if (path.back() != peer_id)
    {
        LOG_DEBUG << "peer id is not the last path id";
        return false;
    }

    std::string my_id = ConfManager::instance().GetNodeId();
    if (std::find(path.begin(), path.end(), my_id) != path.end())
    {
        LOG_DEBUG << "my id is already in the path";
        return false;
    }


    auto it = std::unique (path.begin(), path.end());
    path.resize( std::distance(path.begin(),it) );

    if(path_len != path.size())
    {
        LOG_DEBUG << "path has duplicate elements";
        return false;
    }

    return true;
}

int32_t matrix_socket_channel_handler::on_read(network::channel_handler_context &ctx, byte_buf &in)
{
    if (in.get_valid_read_len() > 0)
    {
        decode_status recv_status = m_decoder->recv_message(in);

        if (recv_status != DECODERR_SUCCESS)
        {
            LOG_ERROR << "recv error msg." << m_sid.to_string();
            return recv_status;
        }

        if (recv_status != DECODERR_SUCCESS )
        {
            return recv_status;
        }

        while (m_decoder->has_complete_message())
        {
            set_decode_context(ctx);

            std::shared_ptr<network::message> msg = std::make_shared<network::message>();
            decode_status status = m_decoder->decode(ctx, msg);

            //decode success
            if (DECODERR_SUCCESS == status)
            {
                //modify by regulus: fix ver_req duplication error.
                //callback
                msg->header.src_sid = m_sid;

                int32_t iprocRet = on_after_msg_received(*msg);
                if (iprocRet != ERR_SUCCESS)
                {
                    return iprocRet;
                }

                //send to bus


                if (msg->get_name() == SHAKE_HAND_REQ
                    || msg->get_name() == SHAKE_HAND_RESP)
                {
                    // not deliver shake hand message to upper service

                }
                else
                {

                    if (nullptr == msg || nullptr == msg->content)
                    {
                        LOG_ERROR << "decode error, msg is null" << m_sid.to_string();
                        return DECODE_ERROR;
                    }

                    if (!validate_req_path(msg->get_name(), msg->content->header.path))
                    {
                        LOG_DEBUG << "req path validate fail, drop it";
                        return ERR_SUCCESS;
                    }


                    if(!validate_resp_path(msg->get_name(), msg->content->header.path))
                    {
                        LOG_DEBUG << "resp path validate fail, drop it";
                        return ERR_SUCCESS;
                    }


                    std::string nonce = msg->content->header.__isset.nonce ? msg->content->header.nonce : "";
                    //check msg duplicated
                    if (!service_proto_filter::get_mutable_instance().check_dup(nonce))
                    {
                        if (m_f_ctl != nullptr && m_f_ctl->over_speed(1))
                        {
                            LOG_INFO << "over_speed";
                            return ERR_SUCCESS;
                        }


//                                if (std::dynamic_pointer_cast<matrix::core::msg_forward>(msg->content))
                        if (msg->get_name() == std::string(BINARY_FORWARD_MSG))
                        {
//                                    msg->set_name(BINARY_FORWARD_MSG);

                            topic_manager::instance().publish<void>(BINARY_FORWARD_MSG, msg);
                            LOG_DEBUG << "matrix socket channel handler forward msg: " << msg->get_name()
                                      << ", nonce: " << nonce << m_sid.to_string();
                        }
                        else
                        {
                            topic_manager::instance().publish<void>(msg->get_name(), msg);
                            //LOG_INFO << "m_count "<< m_count << endl;
                            LOG_DEBUG << "matrix socket channel handler received msg: " << msg->get_name()
                                      << ", nonce: " << nonce << m_sid.to_string();
                        }


                    }
                    else
                    {
                        LOG_DEBUG << "matrix socket channel handler received duplicated msg: " << msg->get_name() << ", nonce: " << nonce << m_sid.to_string();
                    }
                }

                //has message
                set_has_message(*msg);
            }
            //not enough, return and next time
            else if (DECODE_UNKNOWN_MSG == status)
            {
                LOG_DEBUG << "unknow message: " << msg->get_name();
            }
            else if (DECODE_NEED_MORE_DATA == status)
            {
                LOG_DEBUG << "package contiue to receive";
            }
            //decode error
            else
            {
                LOG_ERROR << "matrix socket channel handler on read error and call socket channel on_error, " << m_sid.to_string();
                return status;
            }
        }

    }
    return ERR_SUCCESS;
}

void matrix_socket_channel_handler::set_decode_context(network::channel_handler_context &ctx)
{
    std::string node_id = ConfManager::instance().GetNodeId();
    variable_value v1;
    v1.value() = node_id;
    ctx.add("LOCAL_NODE_ID", v1);
}

void matrix_socket_channel_handler::set_encode_context(network::channel_handler_context &ctx)
{
    if (auto ch_ = m_channel.lock())
    {
        std::shared_ptr<network::tcp_socket_channel> ch = std::dynamic_pointer_cast<network::tcp_socket_channel>(ch_);
        if (nullptr != ch)
        {
            auto a = ch->get_proto_capacity();
            auto b = ConfManager::instance().get_proto_capacity();

            bool compress_enabled = network::matrix_capacity_helper::get_compress_enabled(a , b);
            int  thrift_proto = network::matrix_capacity_helper::get_thrift_proto(a , b);

            variable_value v1;
            v1.value() = compress_enabled;
            ctx.add("ENABLE_COMPRESS", v1);

            variable_value v2;
            v2.value() = thrift_proto;
            ctx.add("THRIFT_PROTO", v2);
        }
    }

}

int32_t matrix_socket_channel_handler::on_write(network::channel_handler_context &ctx, 
    network::message &msg, byte_buf &buf)
{
    LOG_DEBUG << "socket channel handler send msg: " << msg.get_name() << m_sid.to_string();

    set_encode_context(ctx);

    encode_status status = m_coder->encode(ctx, msg, buf);

    if (ENCODERR_SUCCESS == status)
    {
        if (msg.get_name() != SHAKE_HAND_REQ
            && msg.get_name() != SHAKE_HAND_RESP)
        {
            std::string nonce = (msg.content)->header.__isset.nonce ? (msg.content)->header.nonce : "";
            //insert nonce to avoid receive msg sent by itself
            service_proto_filter::get_mutable_instance().insert_nonce(nonce);

            LOG_DEBUG << m_sid.to_string() << " matrix socket channel handler send msg: " << msg.get_name() << ", nonce: " << nonce << " buf message:" << buf.to_string();
        }

        //has message
        set_has_message(msg);

        return ERR_SUCCESS;
    }
    else if (BUFFER_IS_NOT_ENOUGH_TO_ENCODE == status)
    {
        //this should be found in coding phase
        LOG_ERROR << "matrix_socket_channel_handler encode error, buffer is not enough to encode: " << msg.get_name();
        return E_DEFAULT;
    }
    else
    {
        LOG_ERROR << "matrix_socket_channel_handler encode error: " << msg.get_name();
        return E_DEFAULT;
    }
}

int32_t matrix_socket_channel_handler::on_error()
{
    if (auto ch = m_channel.lock())
    {
        ch->on_error();
    }
    return ERR_SUCCESS;
}

void matrix_socket_channel_handler::start_shake_hand_timer()
{
    //m_shake_hand_timer.expires_from_now(std::chrono::seconds(SHAKE_HAND_INTERVAL));
    //m_shake_hand_timer.async_wait(m_shake_hand_timer_handler);
    start_shake_hand_timer_ext();
    reset_has_message();
}

void matrix_socket_channel_handler::stop_shake_hand_timer()
{
    boost::system::error_code error;

    m_shake_hand_timer.cancel(error);
    if (error)
    {
        LOG_ERROR << "matrix socket channel handler cancel timer error: " << error;
    }

    //modify by regulus:fix can't free resource when client disconnect
    //m_shake_hand_timer_handler = nullptr;

}

void matrix_socket_channel_handler::set_has_message(network::message &msg)
{
    if (SHAKE_HAND_REQ == msg.get_name()
        || SHAKE_HAND_RESP == msg.get_name()
        || VER_REQ == msg.get_name()
        || VER_RESP == msg.get_name())
    {
        return;
    }

    m_has_message = true;
}
