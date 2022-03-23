#ifndef DBC_NETWORK_HANDLER_CREATE_FUNCTOR_H
#define DBC_NETWORK_HANDLER_CREATE_FUNCTOR_H

#include "channel.h"
#include "socket_channel_handler.h"

typedef  std::function<std::shared_ptr<network::socket_channel_handler> 
	(std::shared_ptr<network::channel> ch)> handler_create_functor;

#endif
