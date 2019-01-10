#include <path_follower/initiator.h>

int main(int _argc, char **_argv) {
    ros::init(_argc, _argv, "initiator_node");

    Initiator initiator;
    ros::Rate rate(5);
    while (ros::ok()) {
        ros::spinOnce();
        initiator.pubMsgs();
        rate.sleep();
    }

    return 0;
}