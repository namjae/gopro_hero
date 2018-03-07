/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*- */

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/regex.hpp>
#include <boost/thread/thread.hpp>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>

#include <iostream>

#include <ros/console.h>

#include "gopro_hero/gopro_hero.hpp"

using namespace std;

/*! \file gopro_hero.cpp
    \brief Controls all camera functionality except for streaming
*/

namespace gopro_hero
{

    /// Constructor
    /// \note enable curl
    GoProHero::GoProHero() :
        saveOnDevice_(true), 
        mode_(Mode::PHOTO)
    {
        curl_global_init(CURL_GLOBAL_ALL);

        if (std::getenv("IMAGE_LIST_FROM") != nullptr) {
            std::string from(std::getenv("IMAGE_LIST_FROM"));
            if (boost::iequals("JSON", from)) {
                imageListFrom_ = JSON;
            } else if (boost::iequals("HTML", from)) {
                imageListFrom_ = HTML;
            } else {
                ROS_INFO_STREAM("Unknown format:" << from);
                ROS_INFO_STREAM("Using default format: HTML");
                imageListFrom_ = HTML;
            }
        } else {
            ROS_DEBUG_STREAM("IMAGE_LIST_FROM env var not specified.");
            ROS_DEBUG_STREAM("Using default format: HTML");
            imageListFrom_ = HTML;
        }
    }

    /// Destructor
    /// \note clean up curl
    GoProHero::~GoProHero()
    {
        curl_global_cleanup();
    }


    /// Retrieves a list of images from the camera
    /// \param images the list of images, each as a vector of bytes
    /// \param timeout the amount of time to wait for the images
    /// \todo verify that images are jpegs
    void GoProHero::currentImages(vector<vector<unsigned char> >& images, long timeout) {

        switch (imageListFrom_) {
        case JSON: {
            // JSON media list(http://10.5.5.9/gp/gpMediaList) is NOT WORKING for now(2018.3.6).
            Json::Value root;
            Json::Reader reader;
            std::string mediaList;
            if (!curlGetText("http://10.5.5.9/gp/gpMediaList", mediaList, 2)) {
                ROS_ERROR_STREAM("FAILED to get image list!!");
                return;
            }
            ROS_DEBUG_STREAM("gpMediaList:" << mediaList);

            if (!mediaList.empty() && reader.parse(mediaList, root))
                {
#if 0
                    const Json::Value media = root["media"][0]["fs"];
                    const Json::Value lastVal = media[media.size() - 1];
            
                    // TODO Check that it's a JPG
            
                    int startNum = stoi(lastVal["b"].asString());
                    int endNum = stoi(lastVal["l"].asString());
                    for (int i=startNum; i<=endNum; ++i)
                        {
                            string path = "http://10.5.5.9/videos/DCIM/100GOPRO/G" +
                                zeroPaddedIntString(lastVal["g"].asString(), 3) +
                                zeroPaddedIntString(to_string(i), 4) + ".JPG";
                            ROS_DEBUG_STREAM("getting: " << path);
                    
                            vector<unsigned char> image;
                            curlGetBytes(path, image, timeout);
                            images.push_back(image);
                        }
#else
                    // JSON format for now(2018.3.6)
                    const Json::Value media = root["media"][0]["fs"];
                    for (int i = 0; i < media.size(); ++i) {
                        string path = "http://10.5.5.9/videos/DCIM/100GOPRO/" +
                            media[i]["n"].asString();
                        ROS_DEBUG_STREAM("getting: " << path);
                        vector<unsigned char> image;
                        curlGetBytes(path, image, timeout);
                        images.push_back(image);
                    }
#endif
                } else {
                ROS_ERROR_STREAM("empty media list or media list parsing error");
            }
        }
            break;
        case HTML: {
            std::string htmlMediaList;
            if (!curlGetText("http://10.5.5.9/videos/DCIM/100GOPRO/", htmlMediaList, 2)) {
                ROS_ERROR_STREAM("FAILED to get image list!!");
                return;
            }
            vector<std::string> imageFiles;
            findImageFiles(htmlMediaList, imageFiles);
            
            if (!imageFiles.empty()) {
                for (int i = 0; i < imageFiles.size(); ++i) {
                    string path = "http://10.5.5.9/videos/DCIM/100GOPRO/" + imageFiles[i];
                    ROS_DEBUG_STREAM("getting: " << path);
                    vector<unsigned char> image;
                    curlGetBytes(path, image, timeout);
                    images.push_back(image);
                }
            } else {
                ROS_ERROR_STREAM("empty media list or media list page parsing error");
            }
            break;
        }
        }
    }


    /// Set the camera's primary mode (video, photo, multishot)
    /// \param m PrimaryMode enum class
    void GoProHero::setMode(Mode m)
    {
        mode_ = m;
        switch (m) {
        case Mode::VIDEO:
        {
            sendCommand("mode?p=0"); // set as mode
            sendSetting("10/1"); // turn on
            break;
        }
        case Mode::PHOTO:
        {
            sendCommand("mode?p=1");
            sendSetting("21/1");
            break;
        }
        case Mode::MULTISHOT:
        {
            sendCommand("mode?p=2");
            sendSetting("34/1");
            break;
        }
        default: break;
        }
    }


    /// Sends a wake-on-lan packet to the camera
    /// \param mac mac address of YOUR network adapter
    /// \link https://en.wikipedia.org/wiki/Wake-on-LAN
    void GoProHero::sendMagicPacket(array<unsigned char, 6> mac)
    {
        using namespace boost::asio;
        
        array<unsigned char, 102> buf;
        for (int i=0; i<6; ++i) buf[i] = 0xFF; // 6 bytes
        for (int i=1; i<17; ++i) memcpy(&buf[i*6], &mac, 6 * sizeof(unsigned char)); // 96 bytes
        
        // send as UDP packet
        io_service ioService;
        ip::udp::socket socket(ioService);
        ip::udp::endpoint remoteEndpoint;
            
        socket.open(ip::udp::v4());
        remoteEndpoint = ip::udp::endpoint(ip::address::from_string("10.5.5.9"), 9);
        socket.send_to(buffer(buf), remoteEndpoint); //, 0, err);
        socket.close();
    }

    /// Sends a string of bytes to the camera
    /// \param a string that holds an array of bytes
    /// \return success condition
    /// \todo Accept and parse output for success/failure--
    /// \todo Catch exceptions
    bool GoProHero::send(string s)
    {
        string empty;
        curlGetText(s, empty, 2);
        return true;
    }


    /// Get a list of images from the camera
    /// \param url the camera's control/setting url
    /// \param image a vector of images (byte arrays)
    /// \param timeout attempt the read for this many ms
    /// \return success condition
    bool GoProHero::curlGetBytes(const string url, vector<unsigned char>& image, long timeout)
    {
        string s;
        if (curlRequestUrl(url, s, timeout))
        {
            copy(s.begin(), s.end(), back_inserter(image));
            return true;
        }
        return false;
    }


    /// A curl-specific callback for reading packets
    /// \return the size of the packet read
    size_t GoProHero::curlWriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
    {
        ((string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }


    /// Retrieve text from a requested url
    /// \param url the command/setting url
    /// \param text the retrieved text
    /// \param timeout the amount of time to wait for request response
    /// \return success condition
    bool GoProHero::curlGetText(const string url, string& text, long timeout)
    {
        return curlRequestUrl(url, text, timeout);
    }


    /// Request a url and fill a buffer with the response
    /// \param url the command/setting url
    /// \param readBuffer the buffer into which to put the response
    /// \param timeout the amount of time to wait for response
    /// \return success condition
    bool GoProHero::curlRequestUrl(const string url, string& readBuffer, long timeout)
    {
        CURL* curl = curl_easy_init();
        CURLcode res(CURLE_FAILED_INIT);
        
        if (curl)
        {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &GoProHero::curlWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
            res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
        }
        return true; //CURLE_OK == res;
    }


    /// Pad a string with zeroes at the beginning
    /// \param num the string to pad
    /// \param pad the number of zeroes
    /// \return the new padded string
    string GoProHero::zeroPaddedIntString(string num, int pad)
    {
        ostringstream ss;
        ss << setw(pad) << setfill('0') << num;
        return ss.str();
    }

    /// Find image files in html page
    /// \param htmlMediaList html page contents
    /// \param[out] imageList images found
    void GoProHero::findImageFiles(const std::string& htmlMediaList, vector<std::string>& imageFiles)
    {
        try {
            const boost::regex e("GOPR\\d\\d\\d\\d\\.JPG");
            boost::match_results<std::string::const_iterator> m;
            std::string::const_iterator start = htmlMediaList.begin();
            std::string::const_iterator end = htmlMediaList.end();

            while (boost::regex_search(start, end, m, e)) {
                start = m[0].second;
                std::string img = htmlMediaList.substr(std::distance(htmlMediaList.begin(), m[0].first), 12);
                ROS_DEBUG_STREAM("found image:" << img);
                imageFiles.push_back(img);
            }
        }
        catch (std::exception& e) {
            cerr << e.what() << endl;
        }
    }

}
