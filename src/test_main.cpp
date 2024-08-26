
#include "messagebus.h"
#include "timer_mgr.h"
#include "data_proxy/common.h"


int main()
{
    MessageBus<InternalMessage> message_bus;
	CoExecutor co_executor(4);
	co_executor.start();

    TimerMgr time_mgr(&message_bus, &co_executor);

	message_bus.run();
}