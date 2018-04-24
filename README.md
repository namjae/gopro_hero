# gopro_hero
ROS package for GoPro cameras Hero4 and up

# build

. /opt/ros/kinetic/setup.sh
sudo apt install libavdevice-dev
mkdir -p ~/ws/src
cd $_
catkin_init_workspace
git clone https://github.com/teevr/gopro_hero -b develop
cd ..
sudo mkdir -p /opt/local/ros
catkin_make install -j1 -DCMAKE_INSTALL_PREFIX=/opt/local/ros/gopro_hero
cd /opt/local/ros
tar cvzf gopro_hero.tgz gopro_hero
