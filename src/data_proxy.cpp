#include "logic/logic_mgr.h"
#include "logic/scheduler_logic.h"
#include "logic/test.h"




int main(int argc, char** argv)
{
	rclcpp::init(argc, argv, rclcpp::InitOptions(), rclcpp::SignalHandlerOptions::None);
	LogicMgr logic;
	logic.add_logic<SchedulerLogic>("schedulerlogic");
	logic.add_logic<Test>("Test");
	logic.start();
	return 0;
}
