#include "server_initiator_factory.h"
#include "server_initiator.h"

create_functor_type server_initiator_factory::m_initiator_creator = std::function<server_initiator*()>(nullptr);

server_initiator *server_initiator_factory::create_initiator()
{
    return m_initiator_creator();           //bind and lazy execute to avoid service invasion
}
