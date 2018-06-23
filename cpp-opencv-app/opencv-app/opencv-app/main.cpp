#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>

#include <include/sharedmemory.hpp>
#include <include/lidar.hpp>
#include <include/lanedetector.hpp>
#include <include/stoplights.h>
#include <include/ids.h>

// Race mode ---> fps boost
// Debug mode --> display data
//#define RACE_MODE
#define DEBUG_MODE
#define NO_USB
//#define STOPLIGHTS_MODE
#define IDS_MODE

#define CAMERA_INDEX 0
#define CAM_RES_X 640
#define CAM_RES_Y 480
#define FRAME_TIME 30

// Handlers for custom classes
LaneDetector laneDetector;
StopLightDetector lightDetector;
Lidar lidar;
SharedMemory shm_lidar_data(50001);
SharedMemory shm_lane_points(50002);
SharedMemory shm_usb_to_send(50003);
SharedMemory shm_watchdog(50004);
IDS_PARAMETERS ids_camera;
HIDS hCam = 1;

// Variables for communication between threads
bool close_app = false;
std::mutex mu;

// Function declarations
//

int main()
{
     cv::Mat test(480, 640, CV_8UC3);
#ifdef DEBUG_MODE
    //FPS
    struct timespec start, end;
    unsigned int licznik_czas = 0;
    float seconds = 0;
    float fps = 0;
#endif

    // Declaration of cv::MAT variables
    cv::Mat frame(CAM_RES_Y, CAM_RES_X, CV_8UC4);
    cv::Mat frame_ids(752,480,CV_8UC3);
    cv::Mat yellow_bird_eye_frame(CAM_RES_Y, CAM_RES_X, CV_8UC3);
    cv::Mat white_bird_eye_frame(CAM_RES_Y, CAM_RES_X, CV_8UC3);
    cv::Mat yellow_bird_eye_frame_tr(CAM_RES_Y, CAM_RES_X, CV_8UC3);
    cv::Mat white_bird_eye_frame_tr(CAM_RES_Y, CAM_RES_X, CV_8UC3);
    cv::Mat yellow_vector_frame(CAM_RES_Y, CAM_RES_X, CV_8UC3);
    cv::Mat white_vector_frame(CAM_RES_Y, CAM_RES_X, CV_8UC3);
    cv::Mat undist_frame, cameraMatrix, distCoeffs;
    cv::Mat cone_frame_out;
    cv::Mat frame_out_yellow, frame_out_white, frame_out_edge_yellow, frame_out_edge_white;

#ifdef STOPLIGHTS_MODE
    //cv::Mats used in stoplight algorythm
    cv::Mat old_frame(CAM_RES_Y,CAM_RES_X,CV_8UC1);
    cv::Mat display;
    cv::Mat diffrence(CAM_RES_Y,CAM_RES_X,CV_8UC1);
#endif

    // Declaration of std::vector variables
    std::vector<std::vector<cv::Point>> yellow_vector;
    std::vector<std::vector<cv::Point>> white_vector;

#ifdef IDS_MODE
    //cvNamedWindow("frame_ids",1);
    ids_camera.initialize_camera(&hCam);
    ids_camera.setting_auto_params(&hCam);
    ids_camera.change_params(&hCam);
#ifdef DEBUG_MODE
    ids_camera.create_trackbars();
#endif
#endif

#ifndef IDS_MODE
    // Camera init
    cv::VideoCapture camera;
    camera.open(CAMERA_INDEX, cv::CAP_V4L2);

    // Check if camera is opened propely
    if (!camera.isOpened())
    {
        std::cout << "Error could not open camera on port: " << CAMERA_INDEX << std::endl
                  << "Closing app!" << std::endl;

        camera.release();
        return 0;
    }
    else
    {
        // Set resolution
        camera.set(cv::CAP_PROP_FRAME_WIDTH, CAM_RES_X);
        camera.set(cv::CAP_PROP_FRAME_HEIGHT, CAM_RES_Y);

        //Set and check decoding of frames
        camera.set(cv::CAP_PROP_CONVERT_RGB, true);
        std::cout << "RGB: " << camera.get(cv::CAP_PROP_CONVERT_RGB) << std::endl;
    }
#endif

    //Read XML file
    laneDetector.UndistXML(cameraMatrix, distCoeffs);
    laneDetector.CreateTrackbars();

#ifndef NO_USB
    // Lidar communication init
    lidar.init();
    lidar.allocate_memory();
    lidar.calculate_offset();
#endif

    // Shared Memory init
    shm_lidar_data.init();
    shm_lane_points.init();
    shm_usb_to_send.init();
    shm_watchdog.init();

    //Trackbars
    //

/*
    //Read from file
    cv::Mat frame_gray(CAM_RES_Y, CAM_RES_X, CV_8UC1);
    frame_gray = cv::imread("../../data/SELFIE-example-img-prosto.png", CV_LOAD_IMAGE_GRAYSCALE);

    if(!frame_gray.data)
    {
        std::cout << "No data!" << std::endl
                  << "Closing app!" << std::endl;

        return 0;
    }
*/
#ifdef STOPLIGHTS_MODE
    camera >>frame;
    lightDetector.prepare_first_image(frame,old_frame,lightDetector.roi_number);
    cv::namedWindow("Light detection", 1);
    cv::namedWindow("Frame", 1);
    while(lightDetector.start_light ==false){
         camera >> frame;
        //Test ROI on input frame
        //lightDetector.test_roi(frame,display);
        lightDetector.find_start(frame,diffrence,old_frame,lightDetector.roi_number);
#ifdef DEBUG_MODE
        if (lightDetector.start_finding ==true){
            cv::imshow("Frame",frame);
            cv::imshow("Light detection",diffrence);
            //cv::imshow("Light detection", display);
        }
        if (lightDetector.start_light == true){
            std::cout<<"START"<<std::endl;
        }
        else
            std::cout<<"WAIT"<<std::endl;

        cv::waitKey(20);

#endif
    }

#endif

    // Main loop
    while(true)
    {
#ifdef DEBUG_MODE
        //FPS
        if(licznik_czas == 0)
        {
            clock_gettime(CLOCK_MONOTONIC, &start);
        }
        //FPS
#endif

        // Get new frame from camera

#ifndef IDS_MODE
        camera >> frame;
#endif

#ifdef IDS_MODE
        cv::Mat ids_image (480, 752, CV_8UC3);
        ids_camera.get_frame(&hCam,752,480,ids_image);
        ids_camera.update_params(&hCam);
        //writes how many fps we have
        //std::cout << "FPS: "<<ids_camera.NEWFPS<< std::endl;

        //other way doesn't work
        ids_image.copyTo(frame_ids);

#endif


        // Process frame
#ifndef IDS_MODE
        laneDetector.Undist(frame, undist_frame, cameraMatrix, distCoeffs);
#endif
#ifdef IDS_MODE
        laneDetector.Undist(frame_ids, undist_frame, cameraMatrix, distCoeffs);
#endif
        laneDetector.Hsv(undist_frame, frame_out_yellow, frame_out_white, frame_out_edge_yellow, frame_out_edge_white);
        laneDetector.BirdEye(frame_out_edge_yellow, yellow_bird_eye_frame);
        laneDetector.BirdEye(frame_out_edge_white, white_bird_eye_frame);
        laneDetector.colorTransform(yellow_bird_eye_frame, yellow_bird_eye_frame_tr);
        laneDetector.colorTransform(white_bird_eye_frame, white_bird_eye_frame_tr);

        // Detect cones
        std::vector<cv::Point> cones_vector;
        laneDetector.ConeDetection(frame, cone_frame_out, cones_vector);

        // Detect lines
        laneDetector.detectLine(yellow_bird_eye_frame, yellow_vector);
        laneDetector.detectLine(white_bird_eye_frame_tr, white_vector);
        laneDetector.drawPoints(yellow_vector, yellow_vector_frame);
        laneDetector.drawPoints(white_vector, white_vector_frame);

        // Push data
        std::cout << "Push" << std::endl;
        shm_lane_points.push_data(yellow_vector, white_vector, white_vector); // <-- Do zmiany trzeci wektor z pacholkami
        std::cout << "Pull" << std::endl;
        //shm_lane_points.pull_data(test);


#ifdef DEBUG_MODE
        // Display info on screen
        //cv::imshow("Camera", frame);
        //cv::imshow("UndsCamera", undist_frame);
        cv::imshow("0 Frame", frame_ids);
        cv::imshow("1.1 Yellow Line", frame_out_yellow);
        cv::imshow("1.2 White Line", frame_out_white);
        cv::imshow("2.1 Yellow Edges", frame_out_edge_yellow);
        cv::imshow("2.2 White Edges", frame_out_edge_white);
        //cv::imshow("BirdEyeTtransform", bird_eye_frame_tr);
        cv::imshow("3.1 Yellow Bird Eye", yellow_bird_eye_frame);
        cv::imshow("3.2 White Bird Eye", white_bird_eye_frame);
        cv::imshow("4.1 Yellow Vector", yellow_vector_frame);
        cv::imshow("4.2 White Vector", white_vector_frame);
        cv::imshow("5 Cone Detect", cone_frame_out);

        cv::imshow("TEST", test);
        test = cv::Mat::zeros(CAM_RES_Y, CAM_RES_X, CV_8UC3);

        yellow_vector_frame = cv::Mat::zeros(CAM_RES_Y, CAM_RES_X, CV_8UC3);
        white_vector_frame = cv::Mat::zeros(CAM_RES_Y, CAM_RES_X, CV_8UC3);

        // Get input from user
        char keypressed = (char)cv::waitKey(FRAME_TIME);

        // Process given input
        if( keypressed == 27 )
            break;

        switch(keypressed)
        {
        case '1':
        {
            cv::Mat LIDAR_data = cv::Mat(FRAME_X, FRAME_Y, CV_8UC3);

            while(true)
            {
                lidar.read_new_data();
                lidar.polar_to_cartesian();
                lidar.draw_data(LIDAR_data);

                // Show data in window
                cv::imshow("LIDAR", LIDAR_data);
                // Clear window for next frame data
                LIDAR_data = cv::Mat::zeros(FRAME_X, FRAME_Y, CV_8UC3);

                // Get input from user
                char keypressed_1 = (char)cv::waitKey(100);

                // Process given input
                if( keypressed_1 == 27 )
                    break;
            }


            break;
        }
        case '2':

            break;

        default:

            break;
        }

        if(licznik_czas > 1000)
        {
            licznik_czas = 0;
            clock_gettime(CLOCK_MONOTONIC, &end);
            seconds = (end.tv_sec - start.tv_sec);
            fps  =  1 / (seconds / 1000);
            std::cout << "FPS: " << fps << std::endl;
        }
        else
        {
            licznik_czas++;
        }

#endif


    }

    close_app = true;
#ifndef IDS_MODE
    camera.release();
#endif

#ifdef IDS_MODE
    is_ExitCamera(hCam);
#endif

    shm_lidar_data.close();
    shm_lane_points.close();
    shm_usb_to_send.close();
    shm_watchdog.close();

    return 0;
}
