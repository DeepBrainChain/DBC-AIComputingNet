#include "idle_task_message.h"

std::string idle_task_req::to_string()
{
    rapidjson::Document document;
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    rapidjson::Value root(rapidjson::kObjectType);

    root.AddMember("mining_node_id", STRING_REF(mining_node_id), allocator);
    root.AddMember("time_stamp", STRING_REF(time_stamp), allocator);

    root.AddMember("sign_type", STRING_REF(sign_type), allocator);
    root.AddMember("sign", STRING_REF(sign), allocator);

    std::shared_ptr<rapidjson::StringBuffer> buffer = std::make_shared<rapidjson::StringBuffer>();
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(*buffer);
    root.Accept(writer);

    return buffer->GetString();
}

/*int32_t parse_string_item(rapidjson::Document &doc, std::string item, std::string &target)
{
    if (!doc.HasMember(item.c_str()))
    {
        LOG_ERROR << "parse error. Do not have " << item;
        return E_DEFAULT;
    }
    rapidjson::Value &value = doc[item.c_str()];
    auto ret_type = value.GetType();
    if (ret_type != rapidjson::kStringType)
    {
        return E_DEFAULT;
    }
    target = value.GetString();
    return E_DEFAULT;
}*/
void idle_task_resp::from_string(const std::string & buf)
{
    try
    {
        rapidjson::Document doc;
        //doc.Parse<0>(buf.c_str());              //left to later not all fields set
        if (doc.Parse<0>(buf.c_str()).HasParseError())
        {
            LOG_ERROR << "parse idle_task_resp error:" << GetParseError_En(doc.GetParseError());
            return;
        }
        //parse message
        //status
        if (E_SUCCESS != parse_item_int64(doc, "status", this->status))
        {
            this->status = OSS_NET_ERROR;
            return;
        }
        if (this->status != OSS_SUCCESS)
        {
            return;
        }
        //idle_task_id
        if (E_SUCCESS != parse_item_string(doc, "idle_task_id", this->idle_task_id))
        {
            this->status = OSS_NET_ERROR;
            return;
        }

        //if idle_task_id=0, means clear local idle task, dbc do not need to exec idle_task.
        if (CLEAR_IDLE_TASK == this->idle_task_id )
        {
            return;
        }

        //code_dir
        if (E_SUCCESS != parse_item_string(doc, "code_dir", this->code_dir))
        {
            this->status = OSS_NET_ERROR;
            return;
        }

        //entry_file
        if (E_SUCCESS != parse_item_string(doc, "entry_file", this->entry_file))
        {
            this->status = OSS_NET_ERROR;
            return;
        }

        //training_engine
        if (E_SUCCESS != parse_item_string(doc, "training_engine", this->training_engine))
        {
            this->status = OSS_NET_ERROR;
            return;
        }

        //data_dir
        parse_item_string(doc, "data_dir", this->data_dir);

        //hyper_parameters
        parse_item_string(doc, "hyper_parameters", this->hyper_parameters);

    }
    catch (...)
    {
        LOG_ERROR << "container client inspect container resp exception";
    }
}
