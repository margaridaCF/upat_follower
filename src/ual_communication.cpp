//----------------------------------------------------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018 Hector Perez Leon
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
// Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
// OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//----------------------------------------------------------------------------------------------------------------------

#include <upat_follower/ual_communication.h>

namespace upat_follower {

UALCommunication::UALCommunication() : nh_(), pnh_("~") {
    // Parameters
    pnh_.getParam("uav_id", uav_id_);
    pnh_.getParam("save_test_data", save_test_);
    pnh_.getParam("trajectory", trajectory_);
    pnh_.getParam("path", init_path_name_);
    pnh_.getParam("pkg_name", pkg_name_);
    pnh_.getParam("reach_tolerance", reach_tolerance_);
    pnh_.getParam("use_class", use_class_);
    pnh_.getParam("generator_mode", generator_mode_);
    // Subscriptions
    sub_pose_ = nh_.subscribe("/uav_" + std::to_string(uav_id_) + "/ual/pose", 0, &UALCommunication::ualPoseCallback, this);
    sub_state_ = nh_.subscribe("/uav_" + std::to_string(uav_id_) + "/ual/state", 0, &UALCommunication::ualStateCallback, this);
    sub_velocity_ = nh_.subscribe("/upat_follower/follower/uav_" + std::to_string(uav_id_) + "/output_vel", 0, &UALCommunication::velocityCallback, this);
    // Publishers
    pub_set_pose_ = nh_.advertise<geometry_msgs::PoseStamped>("/uav_" + std::to_string(uav_id_) + "/ual/set_pose", 1000);
    pub_set_velocity_ = nh_.advertise<geometry_msgs::TwistStamped>("/uav_" + std::to_string(uav_id_) + "/ual/set_velocity", 1000);
    // Services
    client_take_off_ = nh_.serviceClient<uav_abstraction_layer::TakeOff>("/uav_" + std::to_string(uav_id_) + "/ual/take_off");
    client_land_ = nh_.serviceClient<uav_abstraction_layer::Land>("/uav_" + std::to_string(uav_id_) + "/ual/land");
    client_prepare_path_ = nh_.serviceClient<upat_follower::PreparePath>("/upat_follower/follower/uav_" + std::to_string(uav_id_) + "/prepare_path");
    client_prepare_trajectory_ = nh_.serviceClient<upat_follower::PrepareTrajectory>("/upat_follower/follower/uav_" + std::to_string(uav_id_) + "/prepare_trajectory");
    client_visualize_ = nh_.serviceClient<upat_follower::Visualize>("/upat_follower/visualization/uav_" + std::to_string(uav_id_) + "/visualize");
    // Flags
    on_path_ = false;
    end_path_ = false;
    // Initialize path
    init_path_ = csvToPath("/" + init_path_name_ + ".csv");
    times_ = csvToVector("/times.csv");
    // Save data
    if (save_test_) {
        std::string pkg_name_path = ros::package::getPath(pkg_name_);
        folder_data_name_ = pkg_name_path + "/tests/splines";
    }
}

UALCommunication::~UALCommunication() {
}

nav_msgs::Path UALCommunication::constructPath(std::vector<double> _wps_x, std::vector<double> _wps_y, std::vector<double> _wps_z,
                                               std::vector<double> _wps_ox, std::vector<double> _wps_oy, std::vector<double> _wps_oz,
                                               std::vector<double> _wps_ow, std::string frame_id) {
    nav_msgs::Path out_path;
    std::vector<geometry_msgs::PoseStamped> poses(_wps_x.size());
    out_path.header.frame_id = frame_id;
    for (int i = 0; i < _wps_x.size(); i++) {
        poses.at(i).pose.position.x = _wps_x[i];
        poses.at(i).pose.position.y = _wps_y[i];
        poses.at(i).pose.position.z = _wps_z[i];
        poses.at(i).pose.orientation.x = _wps_ox[i];
        poses.at(i).pose.orientation.y = _wps_oy[i];
        poses.at(i).pose.orientation.z = _wps_oz[i];
        poses.at(i).pose.orientation.w = _wps_ow[i];
    }
    out_path.poses = poses;
    return out_path;
}

nav_msgs::Path UALCommunication::csvToPath(std::string _file_name) {
    nav_msgs::Path out_path;
    std::string pkg_name_path = ros::package::getPath(pkg_name_);
    std::string folder_name = pkg_name_path + "/config" + _file_name;
    std::fstream read_csv;
    read_csv.open(folder_name);
    std::vector<double> list_x, list_y, list_z, list_ox, list_oy, list_oz, list_ow;
    if (read_csv.is_open()) {
        while (read_csv.good()) {
            std::string x, y, z, ox, oy, oz, ow;
            double dx, dy, dz, dox, doy, doz, dow;
            getline(read_csv, x, ',');
            getline(read_csv, y, ',');
            getline(read_csv, z, ',');
            getline(read_csv, ox, ',');
            getline(read_csv, oy, ',');
            getline(read_csv, oz, ',');
            getline(read_csv, ow, '\n');
            std::stringstream sx(x);
            std::stringstream sy(y);
            std::stringstream sz(z);
            std::stringstream sox(ox);
            std::stringstream soy(oy);
            std::stringstream soz(oz);
            std::stringstream sow(ow);
            sx >> dx;
            sy >> dy;
            sz >> dz;
            sox >> dox;
            soy >> doy;
            soz >> doz;
            sow >> dow;
            list_x.push_back(dx);
            list_y.push_back(dy);
            list_z.push_back(dz);
            list_ox.push_back(dox);
            list_oy.push_back(doy);
            list_oz.push_back(doz);
            list_ow.push_back(dow);
        }
    }

    return constructPath(list_x, list_y, list_z, list_ox, list_oy, list_oz, list_ow, "uav_" + std::to_string(uav_id_) + "_home");
}

std::vector<double> UALCommunication::csvToVector(std::string _file_name) {
    std::vector<double> out_vector;
    std::string pkg_name_path = ros::package::getPath(pkg_name_);
    std::string folder_name = pkg_name_path + "/config" + _file_name;
    std::fstream read_csv;
    read_csv.open(folder_name);
    if (read_csv.is_open()) {
        while (read_csv.good()) {
            std::string x;
            double dx;
            getline(read_csv, x, '\n');
            std::stringstream sx(x);
            sx >> dx;
            out_vector.push_back(dx);
        }
    }

    return out_vector;
}

void UALCommunication::ualPoseCallback(const geometry_msgs::PoseStamped::ConstPtr &_ual_pose) {
    ual_pose_ = *_ual_pose;
}

void UALCommunication::ualStateCallback(const uav_abstraction_layer::State &_ual_state) {
    ual_state_.state = _ual_state.state;
}

void UALCommunication::velocityCallback(const geometry_msgs::TwistStamped &_velocity) {
    velocity_ = _velocity;
}

void UALCommunication::saveDataForTesting() {
    static upat_follower::Follower follower_save_tests(uav_id_);
    std::ofstream csv_cubic_loyal, csv_cubic, csv_interp1, csv_init, csv_trajectory;
    target_path_ = follower_save_tests.prepareTrajectory(init_path_, times_);
    csv_trajectory.open(folder_data_name_ + "/trajectory.csv");
    csv_trajectory << std::fixed << std::setprecision(5);
    for (int i = 0; i < target_path_.poses.size(); i++) {
        csv_trajectory << target_path_.poses.at(i).pose.position.x << ", " << target_path_.poses.at(i).pose.position.y << ", " << target_path_.poses.at(i).pose.position.z << std::endl;
    }
    csv_trajectory.close();
    csv_init.open(folder_data_name_ + "/init.csv");
    csv_init << std::fixed << std::setprecision(5);
    for (int i = 0; i < init_path_.poses.size(); i++) {
        csv_init << init_path_.poses.at(i).pose.position.x << ", " << init_path_.poses.at(i).pose.position.y << ", " << init_path_.poses.at(i).pose.position.z << std::endl;
    }
    csv_init.close();
    target_path_ = follower_save_tests.preparePath(init_path_, 0);
    csv_interp1.open(folder_data_name_ + "/interp1.csv");
    csv_interp1 << std::fixed << std::setprecision(5);
    for (int i = 0; i < target_path_.poses.size(); i++) {
        csv_interp1 << target_path_.poses.at(i).pose.position.x << ", " << target_path_.poses.at(i).pose.position.y << ", " << target_path_.poses.at(i).pose.position.z << std::endl;
    }
    csv_interp1.close();
    target_path_ = follower_save_tests.preparePath(init_path_, 1);
    csv_cubic_loyal.open(folder_data_name_ + "/cubic_spline_loyal.csv");
    csv_cubic_loyal << std::fixed << std::setprecision(5);
    for (int i = 0; i < target_path_.poses.size(); i++) {
        csv_cubic_loyal << target_path_.poses.at(i).pose.position.x << ", " << target_path_.poses.at(i).pose.position.y << ", " << target_path_.poses.at(i).pose.position.z << std::endl;
    }
    csv_cubic_loyal.close();
    target_path_ = follower_save_tests.preparePath(init_path_, 2);
    csv_cubic.open(folder_data_name_ + "/cubic_spline.csv");
    csv_cubic << std::fixed << std::setprecision(5);
    for (int i = 0; i < target_path_.poses.size(); i++) {
        csv_cubic << target_path_.poses.at(i).pose.position.x << ", " << target_path_.poses.at(i).pose.position.y << ", " << target_path_.poses.at(i).pose.position.z << std::endl;
    }
    csv_cubic.close();
}

void UALCommunication::callVisualization() {
    upat_follower::Visualize visualize;
    visualize.request.init_path = init_path_;
    visualize.request.generated_path = target_path_;
    visualize.request.current_path = current_path_;
    client_visualize_.call(visualize);
}

void UALCommunication::runMission() {
    static upat_follower::Follower follower_(uav_id_);

    uav_abstraction_layer::TakeOff take_off;
    uav_abstraction_layer::Land land;
    upat_follower::PreparePath prepare_path;
    upat_follower::PrepareTrajectory prepare_trajectory;
    if (target_path_.poses.size() < 1) {
        if (save_test_) saveDataForTesting();
        if (trajectory_) {
            for (int i = 0; i < times_.size(); i++) {
                std_msgs::Float32 v_percentage;
                v_percentage.data = times_[i];
                prepare_trajectory.request.times.push_back(v_percentage);
            }
            prepare_trajectory.request.init_path = init_path_;
            if (!use_class_) {
                client_prepare_trajectory_.call(prepare_trajectory);
                target_path_ = prepare_trajectory.response.generated_path;
            }
            if (use_class_) target_path_ = follower_.prepareTrajectory(init_path_, times_);
        } else {
            prepare_path.request.init_path = init_path_;
            prepare_path.request.generator_mode.data = 2;
            prepare_path.request.look_ahead.data = 1.2;
            prepare_path.request.cruising_speed.data = 1.0;
            if (!use_class_) {
                client_prepare_path_.call(prepare_path);
                target_path_ = prepare_path.response.generated_path;
            }
            if (use_class_) target_path_ = follower_.preparePath(init_path_, generator_mode_, 0.4, 1.0);
        }
    }

    Eigen::Vector3f current_p, path0_p, path_end_p;
    current_p = Eigen::Vector3f(ual_pose_.pose.position.x, ual_pose_.pose.position.y, ual_pose_.pose.position.z);
    path0_p = Eigen::Vector3f(target_path_.poses.front().pose.position.x, target_path_.poses.front().pose.position.y, target_path_.poses.front().pose.position.z);
    path_end_p = Eigen::Vector3f(target_path_.poses.back().pose.position.x, target_path_.poses.back().pose.position.y, target_path_.poses.back().pose.position.z);
    switch (ual_state_.state) {
        case 2:  // Landed armed
            if (!end_path_) {
                take_off.request.height = 12.5;
                take_off.request.blocking = true;
                client_take_off_.call(take_off);
            }
            break;
        case 3:  // Taking of
            break;
        case 4:  // Flying auto
            if (!end_path_) {
                if (!on_path_) {
                    if ((current_p - path0_p).norm() > reach_tolerance_ * 2) {
                        pub_set_pose_.publish(target_path_.poses.at(0));
                    } else if (reach_tolerance_ > (current_p - path0_p).norm() && !flag_hover_) {
                        pub_set_pose_.publish(target_path_.poses.front());
                        on_path_ = true;
                    }
                } else {
                    if (reach_tolerance_ * 2 > (current_p - path_end_p).norm()) {
                        pub_set_pose_.publish(target_path_.poses.back());
                        on_path_ = false;
                        end_path_ = true;
                    } else {
                        if (use_class_) {
                            follower_.updatePose(ual_pose_);
                            velocity_ = follower_.getVelocity();
                        }
                        pub_set_velocity_.publish(velocity_);
                        current_path_.header.frame_id = ual_pose_.header.frame_id;
                        current_path_.poses.push_back(ual_pose_);
                    }
                }
            } else {
                if (reach_tolerance_ * 2 > (current_p - path_end_p).norm() && (current_p - path_end_p).norm() > reach_tolerance_) {
                    pub_set_pose_.publish(target_path_.poses.back());
                } else {
                    land.request.blocking = true;
                    client_land_.call(land);
                }
            }
            break;
        case 5:  // Landing
            break;
    }
}

}  // namespace upat_follower