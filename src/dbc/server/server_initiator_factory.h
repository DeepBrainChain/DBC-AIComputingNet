#ifndef DBC_SERVER_INITIATOR_FACTORY_H
#define DBC_SERVER_INITIATOR_FACTORY_H

#include <functional>
#include "server_initiator.h"

using create_functor_type = std::function<server_initiator*()>;

class server_initiator_factory
{
public:

    server_initiator_factory() {}

    virtual ~server_initiator_factory() {}

    //lazy create
    static void bind_creator(create_functor_type initiator_creator) { m_initiator_creator = initiator_creator; }

    virtual server_initiator * create_initiator();

protected:

    static create_functor_type m_initiator_creator;
};

#endif
