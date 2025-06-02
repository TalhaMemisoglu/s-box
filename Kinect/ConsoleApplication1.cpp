#include <stdio.h>
#include <Windows.h>
#include <Kinect.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <fstream>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <chrono>
#include <set>
#include <stdlib.h>

// Suppress security warnings
//binary çevirme

//ROI selected: x=572, y=150, width=928, height=683
//1920-1080  

#define FILEPATH "C:\\Users\\Musab\\Desktop\\slope.bin"
#define SCP "scp C:\\Users\\Musab\\Desktop\\slope.bin mehme@192.168.92.129:/home/mehme"
// **************************************************************
// They need to be provided correctly
//
// !!! Filepath that will be used to locate map bin file: C:\\Users\\Musab\\Desktop\\slope.bin
// !!! Server pc is: mehme@192.168.92.129:/home/mehme
// **************************************************************


//depth roi 340- 240 80 80
#define _CRT_SECURE_NO_WARNINGS

// Error handling helper
void CheckResult(HRESULT hr, const char* operation) {
    if (FAILED(hr)) {
        printf("Error during %s: %d\n", operation, hr);
        exit(1);
    }
}

// Depth processing parameters
#define DEPTH_MIN  400     // 0.5 meters
#define DEPTH_MAX 4500     // 4.5 meters
#define MEDIAN_KERNEL_SIZE 3
#define BILATERAL_SIGMA_SPACE 2.0f
#define BILATERAL_SIGMA_DEPTH 30.0f
#define CHANGE_THRESHOLD 50  // in millimeters
#define SIGNIFICANT_CHANGE_PERCENTAGE 3.0
#define STABILITY_TIMEOUT 1000  // Time with no changes before sending update (2 seconds)
#define FORCE_UPDATE_TIMEOUT 8000  // Maximum time before forcing an update (8 seconds)
#define EXPORT_INTERVAL 2000  // Export depth values every 2 seconds
#define PRINT_INTERVAL 5000
#define SUBSAMPLE_X 3
#define SUBSAMPLE_Y 5

int flag_edges = 0;

// Add these global variables after the existing global variables
cv::Rect selectedROI = {
    501,
    202,
    905,
    684
};
/*
selectedROI.x = 562;
selectedROI.y = 188;
selectedROI.width = 917;
selectedROI.height = 677;
*/
bool roiSelected = false;
bool isSelectingROI = false;


// Simple TrackedObject structure
struct TrackedObject {
    cv::Point2f center;
    char type;              // 'R', 'S', 'C'
    bool writtenToFile;     // Has this object been written to file?

    TrackedObject(cv::Point2f c, char t) : center(c), type(t), writtenToFile(false) {}
};

// Global tracker - keeps all objects we've ever seen
std::vector<TrackedObject> globalObjectTracker;
const float OBJECT_MATCH_THRESHOLD = 10.0f;


// Mouse callback function for ROI selection
void onMouseROI(int event, int x, int y, int flags, void* userdata) {
    static cv::Point startPoint;
    static bool drawing = false;
    cv::Mat* image = (cv::Mat*)userdata;
    static cv::Mat tempImage;

    if (event == cv::EVENT_LBUTTONDOWN) {
        drawing = true;
        startPoint = cv::Point(x, y);
        tempImage = image->clone();
    }
    else if (event == cv::EVENT_MOUSEMOVE && drawing) {
        cv::Mat displayImage = tempImage.clone();
        cv::rectangle(displayImage, startPoint, cv::Point(x, y), cv::Scalar(0, 255, 0), 2);
        cv::imshow("Select ROI - Click and drag, press SPACE to confirm, ESC to cancel", displayImage);
    }
    else if (event == cv::EVENT_LBUTTONUP) {
        drawing = false;
        selectedROI = cv::Rect(startPoint, cv::Point(x, y));

        // Ensure ROI is within image bounds
        selectedROI &= cv::Rect(0, 0, image->cols, image->rows);



        // Draw final rectangle
        cv::Mat displayImage = tempImage.clone();
        cv::rectangle(displayImage, selectedROI, cv::Scalar(0, 255, 0), 2);
        cv::putText(displayImage, "Press SPACE to confirm, ESC to cancel",
            cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
        cv::imshow("Select ROI - Click and drag, press SPACE to confirm, ESC to cancel", displayImage);
    }
}

// Function to select ROI from the first frame
bool selectROI(const cv::Mat& colorMat) {
    if (colorMat.empty()) return false;

    cv::Mat display;
    cv::cvtColor(colorMat, display, cv::COLOR_BGRA2BGR);

    // Create window and set mouse callback
    cv::namedWindow("Select ROI - Click and drag, press SPACE to confirm, ESC to cancel", cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback("Select ROI - Click and drag, press SPACE to confirm, ESC to cancel", onMouseROI, &display);

    cv::putText(display, "Click and drag to select ROI for object detection",
        cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
    cv::imshow("Select ROI - Click and drag, press SPACE to confirm, ESC to cancel", display);

    printf("Please select ROI for object detection:\n");
    printf("- Click and drag to select area\n");
    printf("- Press SPACE to confirm selection\n");
    printf("- Press ESC to cancel and use full image\n");

    while (true) {
        int key = cv::waitKey(30) & 0xFF;

        if (key == 32) { // SPACE key
            if (selectedROI.width > 50 && selectedROI.height > 50) {
                printf("ROI selected: x=%d, y=%d, width=%d, height=%d\n",
                    selectedROI.x, selectedROI.y, selectedROI.width, selectedROI.height);
                cv::destroyWindow("Select ROI - Click and drag, press SPACE to confirm, ESC to cancel");
                return true;
            }
            else {
                printf("ROI too small, please select a larger area\n");
            }
        }
        else if (key == 27) { // ESC key
            printf("ROI selection cancelled, using full image\n");
            cv::destroyWindow("Select ROI - Click and drag, press SPACE to confirm, ESC to cancel");
            return false;
        }
    }
}


// Simple structure for detected objects
struct SimpleObject {
    cv::Point2f center;
    std::string shape;  // "Rectangle", "Square", or "Circle"
    double area;
};

// SafeRelease template
template<class T>
inline void SafeRelease(T*& pInterfaceToRelease)
{
    if (pInterfaceToRelease != nullptr)
    {
        pInterfaceToRelease->Release();
        pInterfaceToRelease = nullptr;
    }
}

// Function to apply median filter (reduces noise)
void applyMedianFilter(UINT16* src, UINT16* dst, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Skip border pixels
            if (x < MEDIAN_KERNEL_SIZE / 2 || x >= width - MEDIAN_KERNEL_SIZE / 2 ||
                y < MEDIAN_KERNEL_SIZE / 2 || y >= height - MEDIAN_KERNEL_SIZE / 2) {
                dst[y * width + x] = src[y * width + x];
                continue;
            }

            // Collect neighborhood values
            UINT16 values[MEDIAN_KERNEL_SIZE * MEDIAN_KERNEL_SIZE];
            int validCount = 0;

            for (int ky = -MEDIAN_KERNEL_SIZE / 2; ky <= MEDIAN_KERNEL_SIZE / 2; ky++) {
                for (int kx = -MEDIAN_KERNEL_SIZE / 2; kx <= MEDIAN_KERNEL_SIZE / 2; kx++) {
                    UINT16 val = src[(y + ky) * width + (x + kx)];
                    if (val >= DEPTH_MIN && val <= DEPTH_MAX) {
                        values[validCount++] = val;
                    }
                }
            }

            // Simple sort for median (for small kernel sizes)
            if (validCount > 0) {
                for (int i = 0; i < validCount - 1; i++) {
                    for (int j = i + 1; j < validCount; j++) {
                        if (values[i] > values[j]) {
                            UINT16 temp = values[i];
                            values[i] = values[j];
                            values[j] = temp;
                        }
                    }
                }
                dst[y * width + x] = values[validCount / 2];
            }
            else {
                dst[y * width + x] = src[y * width + x];
            }
        }
    }
}

// Function to apply bilateral filter (preserves edges while smoothing)
void applyBilateralFilter(UINT16* src, UINT16* dst, int width, int height) {
    const float sigmaSpace2 = 2.0f * BILATERAL_SIGMA_SPACE * BILATERAL_SIGMA_SPACE;
    const float sigmaDepth2 = 2.0f * BILATERAL_SIGMA_DEPTH * BILATERAL_SIGMA_DEPTH;
    const int radius = (int)(2.0f * BILATERAL_SIGMA_SPACE);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.0f;
            float weightSum = 0.0f;
            UINT16 centerValue = src[y * width + x];

            // Skip invalid depth values
            if (centerValue < DEPTH_MIN || centerValue > DEPTH_MAX) {
                dst[y * width + x] = centerValue;
                continue;
            }

            for (int ky = -radius; ky <= radius; ky++) {
                int sampleY = y + ky;
                if (sampleY < 0 || sampleY >= height) continue;

                for (int kx = -radius; kx <= radius; kx++) {
                    int sampleX = x + kx;
                    if (sampleX < 0 || sampleX >= width) continue;

                    UINT16 sampleValue = src[sampleY * width + sampleX];
                    if (sampleValue < DEPTH_MIN || sampleValue > DEPTH_MAX) continue;

                    // Calculate spatial weight
                    float spatialDist2 = (float)(kx * kx + ky * ky);
                    float spatialWeight = expf(-spatialDist2 / sigmaSpace2);

                    // Calculate depth weight
                    float depthDist2 = (float)((sampleValue - centerValue) * (sampleValue - centerValue));
                    float depthWeight = expf(-depthDist2 / sigmaDepth2);

                    float weight = spatialWeight * depthWeight;
                    sum += sampleValue * weight;
                    weightSum += weight;
                }
            }

            if (weightSum > 0.0f) {
                dst[y * width + x] = (UINT16)(sum / weightSum);
            }
            else {
                dst[y * width + x] = centerValue;
            }
        }
    }
}

// Function to fill small holes using interpolation
void fillHoles(UINT16* depthData, int width, int height) {
    UINT16* temp = new UINT16[width * height];
    memcpy(temp, depthData, width * height * sizeof(UINT16));

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            int idx = y * width + x;

            // Check if this is a hole (zero or invalid value)
            if (depthData[idx] < DEPTH_MIN || depthData[idx] > DEPTH_MAX) {
                // Count valid neighbors and sum their values
                int validCount = 0;
                int sum = 0;

                for (int ky = -1; ky <= 1; ky++) {
                    for (int kx = -1; kx <= 1; kx++) {
                        if (kx == 0 && ky == 0) continue;

                        UINT16 neighborValue = depthData[(y + ky) * width + (x + kx)];
                        if (neighborValue >= DEPTH_MIN && neighborValue <= DEPTH_MAX) {
                            sum += neighborValue;
                            validCount++;
                        }
                    }
                }

                // If we have valid neighbors, fill the hole with average
                if (validCount >= 4) {  // At least half of the neighbors should be valid
                    temp[idx] = (UINT16)(sum / validCount);
                }
            }
        }
    }

    memcpy(depthData, temp, width * height * sizeof(UINT16));
    delete[] temp;
}

// Function for simple console visualization (^ for close objects, space for far objects)
void visualizeSimpleDepthMap(UINT16* depthData, int width, int height, int step) {
    printf("\n--- Simple Depth Map Visualization ---\n");

    // Define depth ranges for different visualization characters
    const int CLOSE_THRESHOLD = 1700; // 1.5 meters - objects closer than this will be marked with ^

    for (int y = 0; y < height; y += step) {
        for (int x = 0; x < width; x += step) {
            UINT16 depth = depthData[y * width + x];

            if (depth >= DEPTH_MIN && depth <= DEPTH_MAX) {
                if (depth < CLOSE_THRESHOLD) {
                    printf("^"); // Close object
                }
                else {
                    printf(""); // Far object
                }
            }
            else {
                printf(" "); // Invalid depth - show as space
            }
        }
        printf("\n");
    }

    printf("--- End of Map ---\n");
}

// Function to detect changes between frames   !! roi ile bak
bool detectChanges(UINT16* current, UINT16* previous, int width, int height, double* changePercentage) {
    int changedPixels = 0;
    int validComparisons = 0;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;

            // Only compare valid depth values
            if (current[idx] >= DEPTH_MIN && current[idx] <= DEPTH_MAX &&
                previous[idx] >= DEPTH_MIN && previous[idx] <= DEPTH_MAX) {

                validComparisons++;

                // Calculate absolute difference
                int diff = abs((int)current[idx] - (int)previous[idx]);

                // Count as change if difference exceeds threshold
                if (diff > CHANGE_THRESHOLD) {
                    changedPixels++;
                }
            }
        }
    }


    *changePercentage = (validComparisons > 0) ?
        ((double)changedPixels * 100.0 / validComparisons) : 0.0;

    return (*changePercentage > SIGNIFICANT_CHANGE_PERCENTAGE);
}

// Simple color frame processing
void processColorFrame(IColorFrame* pColorFrame, cv::Mat& colorMat) {
    if (!pColorFrame) return;

    IFrameDescription* pFrameDescription = nullptr;
    HRESULT hr = pColorFrame->get_FrameDescription(&pFrameDescription);
    if (FAILED(hr)) return;

    int nWidth = 0, nHeight = 0;
    pFrameDescription->get_Width(&nWidth);
    pFrameDescription->get_Height(&nHeight);
    SafeRelease(pFrameDescription);

    colorMat = cv::Mat(nHeight, nWidth, CV_8UC4);
    UINT nBufferSize = nWidth * nHeight * 4;
    hr = pColorFrame->CopyConvertedFrameDataToArray(nBufferSize, colorMat.data, ColorImageFormat_Bgra);
}

// Much more permissive shape classification
std::string classifyShape(const std::vector<cv::Point>& contour) {
    // Calculate basic properties
    double area = cv::contourArea(contour);
    double perimeter = cv::arcLength(contour, true);

    // Get bounding rectangle and circle
    cv::Rect boundRect = cv::boundingRect(contour);
    cv::Point2f center;
    float radius;
    cv::minEnclosingCircle(contour, center, radius);

    // Calculate multiple metrics for circle detection
    double aspectRatio = (double)boundRect.width / boundRect.height;
    double circularity = 4 * CV_PI * area / (perimeter * perimeter);
    double extent = area / (boundRect.width * boundRect.height);
    double solidity = area / cv::contourArea(cv::Mat(contour));

    // Circle area vs enclosing circle area ratio
    double enclosingCircleArea = CV_PI * radius * radius;
    double areaRatio = area / enclosingCircleArea;

    // Approximate contour with different epsilon for better polygon detection
    std::vector<cv::Point> approx;
    double epsilon = 0.015 * cv::arcLength(contour, true); // Reduced epsilon
    cv::approxPolyDP(contour, approx, epsilon, true);

    // Debug output
    std::cout << "    vertices=" << approx.size()
        << ", aspect=" << aspectRatio
        << ", circularity=" << circularity
        << ", areaRatio=" << areaRatio
        << ", extent=" << extent;

    // Improved circle detection with multiple criteria
    if (circularity > 0.55 &&                    // Good circularity
        aspectRatio >= 0.8 && aspectRatio <= 1.25 && // Nearly square bounding box
        // Fills most of enclosing circle
        extent > 0.6) {                          // Fills most of bounding box
        return "Circle";
    }
    // Square detection (special case of rectangle)
    else if (approx.size() == 4 &&
        aspectRatio >= 0.85 && aspectRatio <= 1.15 &&
        extent > 0.8) {
        return "Square";
    }
    // Rectangle detection
    else if (extent > 0.4 && circularity < 0.6 && area > 150) {
        return "Rectangle";
    }
    else {
        std::cout << "Not classified as circle - "
            << "Circularity: " << circularity
            << ", Aspect: " << aspectRatio
            << ", AreaRatio: " << areaRatio
            << ", Extent: " << extent << std::endl;

        return "Unknown";
    }
}

std::vector<SimpleObject> processThresholdedImage(const cv::Mat& binaryImage, cv::Point2f roiOffset, const std::string& method) {
    std::vector<SimpleObject> objects;

    // Find contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binaryImage, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    printf("  %s method found %zu contours\n", method.c_str(), contours.size());

    for (size_t i = 0; i < contours.size(); i++) {
        double area = cv::contourArea(contours[i]);

        // More flexible area filtering
        if (area < 75 || area > 100000) {
            continue;
        }

        // Get center
        cv::Moments m = cv::moments(contours[i]);
        if (m.m00 == 0) continue;

        SimpleObject obj;
        //düzelt
        obj.center = cv::Point2f((m.m10 / m.m00 + roiOffset.x - 620) / 9.28,
            (m.m01 / m.m00 + roiOffset.y - 155) / 6.83);
        obj.area = area;
        obj.shape = classifyShape(contours[i]);

        if (obj.shape != "Unknown") {
            objects.push_back(obj);
            printf("    %s: %s at (%.1f, %.1f), area=%.1f\n",
                method.c_str(), obj.shape.c_str(), obj.center.x, obj.center.y, obj.area);
        }
    }

    return objects;
}

// Function to remove duplicate objects (objects detected multiple times)
std::vector<SimpleObject> removeDuplicateObjects(const std::vector<SimpleObject>& objects) {
    std::vector<SimpleObject> uniqueObjects;
    const float DISTANCE_THRESHOLD = 1.0f; // Minimum distance between different objects // Yeni değişti

    for (const auto& obj : objects) {
        bool isDuplicate = false;

        for (const auto& existing : uniqueObjects) {
            float distance = sqrt(pow(obj.center.x - existing.center.x, 2) +
                pow(obj.center.y - existing.center.y, 2));

            if (distance < DISTANCE_THRESHOLD) {
                isDuplicate = true;
                // Keep the one with larger area (usually more reliable detection)
                if (obj.area > existing.area) {
                    // Replace the existing one
                    for (auto& unique : uniqueObjects) {
                        if (&unique == &existing) {
                            unique = obj;
                            break;
                        }
                    }
                }
                break;
            }
        }

        if (!isDuplicate) {
            uniqueObjects.push_back(obj);
        }
    }

    return uniqueObjects;
}


// Method 1: Adaptive thresholding approach
std::vector<SimpleObject> detectWithAdaptiveThresholding(const cv::Mat& processImage, cv::Point2f roiOffset) {
    std::vector<SimpleObject> objects;

    // Convert to grayscale
    cv::Mat gray;
    cv::cvtColor(processImage, gray, cv::COLOR_BGR2GRAY);

    // Apply adaptive thresholding (good for different lighting conditions)
    cv::Mat adaptive;
    cv::adaptiveThreshold(gray, adaptive, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 15, 8);

    // Also try inverted adaptive threshold
    cv::Mat adaptiveInv;
    cv::adaptiveThreshold(gray, adaptiveInv, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY_INV, 15, 8);

    // Process both thresholded images
    std::vector<SimpleObject> normalObjects = processThresholdedImage(adaptive, roiOffset, "Adaptive");
    std::vector<SimpleObject> invObjects = processThresholdedImage(adaptiveInv, roiOffset, "AdaptiveInv");

    objects.insert(objects.end(), normalObjects.begin(), normalObjects.end());
    objects.insert(objects.end(), invObjects.begin(), invObjects.end());

    return objects;
}

// Method 2: Color-based segmentation
std::vector<SimpleObject> detectWithColorSegmentation(const cv::Mat& processImage, cv::Point2f roiOffset) {
    std::vector<SimpleObject> objects;

    cv::Mat hsv;
    cv::cvtColor(processImage, hsv, cv::COLOR_BGR2HSV);

    // Define color ranges for blue objects
    cv::Scalar blueLower(100, 50, 50);   // Lower HSV for blue
    cv::Scalar blueUpper(130, 255, 255); // Upper HSV for blue

    cv::Mat blueMask;
    cv::inRange(hsv, blueLower, blueUpper, blueMask);

    // Define ranges for very dark objects (black/dark colors)
    cv::Mat grayForDark;
    cv::cvtColor(processImage, grayForDark, cv::COLOR_BGR2GRAY);
    cv::Mat darkMask;
    cv::threshold(grayForDark, darkMask, 60, 255, cv::THRESH_BINARY_INV); // Dark objects

    // Process each color mask
    std::vector<SimpleObject> blueObjects = processThresholdedImage(blueMask, roiOffset, "Blue");
    std::vector<SimpleObject> darkObjects = processThresholdedImage(darkMask, roiOffset, "Dark");

    objects.insert(objects.end(), blueObjects.begin(), blueObjects.end());
    objects.insert(objects.end(), darkObjects.begin(), darkObjects.end());

    return objects;
}



// Method 3: Multiple edge detection with different parameters
std::vector<SimpleObject> detectWithMultipleEdgeDetection(const cv::Mat& processImage, cv::Point2f roiOffset) {
    std::vector<SimpleObject> objects;

    cv::Mat gray;
    cv::cvtColor(processImage, gray, cv::COLOR_BGR2GRAY);

    // Multiple edge detection approaches
    std::vector<std::pair<int, int>> cannyParams = {
        {30, 100},   // Your current parameters
        {20, 60},    // More sensitive (better for dark objects)
        {50, 150},   // Less sensitive (better for noisy images)
        {10, 50}     // Very sensitive (for very faint edges)
    };

    for (size_t i = 0; i < cannyParams.size(); i++) {
        cv::Mat blurred;
        cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);

        cv::Mat edges;
        cv::Canny(blurred, edges, cannyParams[i].first, cannyParams[i].second);

        // Morphological operations
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
        cv::morphologyEx(edges, edges, cv::MORPH_OPEN, kernel);

        std::string methodName = "Edge" + std::to_string(i);
        std::vector<SimpleObject> edgeObjects = processThresholdedImage(edges, roiOffset, methodName);
        objects.insert(objects.end(), edgeObjects.begin(), edgeObjects.end());

        // Save edge images for debugging
        if (flag_edges <= 4) {
            std::string filename = "edges_" + std::to_string(cannyParams[i].first) + "_" + std::to_string(cannyParams[i].second) + ".jpg";
            cv::imwrite(filename, edges);
        }
    }

    return objects;
}

// Simple object detection with debugging
std::vector<SimpleObject> detectObjects(const cv::Mat& colorMat) {
    std::vector<SimpleObject> allObjects;
    if (colorMat.empty()) return allObjects;

    // Convert to BGR
    cv::Mat bgr;
    cv::cvtColor(colorMat, bgr, cv::COLOR_BGRA2BGR);

    // Apply ROI if selected
    cv::Mat processImage;
    cv::Point2f roiOffset(0, 0);

    if (roiSelected && selectedROI.width > 0 && selectedROI.height > 0) {
        cv::Rect safeROI = selectedROI & cv::Rect(0, 0, bgr.cols, bgr.rows);
        processImage = bgr(safeROI);
        roiOffset = cv::Point2f(safeROI.x, safeROI.y);
        printf("Processing ROI: x=%d, y=%d, width=%d, height=%d\n",
            safeROI.x, safeROI.y, safeROI.width, safeROI.height);
    }
    else {
        processImage = bgr;
        printf("Processing full image\n");
    }

    // Method 1: Adaptive thresholding for different lighting conditions
    std::vector<SimpleObject> adaptiveObjects = detectWithAdaptiveThresholding(processImage, roiOffset);

    // Method 2: Color-based detection for specific colors
    std::vector<SimpleObject> colorObjects = detectWithColorSegmentation(processImage, roiOffset);

    // Method 3: Multiple edge detection with different parameters
    std::vector<SimpleObject> edgeObjects = detectWithMultipleEdgeDetection(processImage, roiOffset);

    // Combine all results and remove duplicates
    allObjects.insert(allObjects.end(), adaptiveObjects.begin(), adaptiveObjects.end());
    allObjects.insert(allObjects.end(), colorObjects.begin(), colorObjects.end());
    allObjects.insert(allObjects.end(), edgeObjects.begin(), edgeObjects.end());

    // Remove duplicate detections (objects too close to each other)
    std::vector<SimpleObject> uniqueObjects = removeDuplicateObjects(allObjects);

    printf("Detection summary: Adaptive=%zu, Color=%zu, Edge=%zu, Final=%zu\n",
        adaptiveObjects.size(), colorObjects.size(), edgeObjects.size(), uniqueObjects.size());

    return uniqueObjects;
}


void saveDetectionResults(const cv::Mat& image, const std::vector<SimpleObject>& objects, int attemptNumber, bool isFinalResult = false) {
    cv::Mat display;
    cv::cvtColor(image, display, cv::COLOR_BGRA2BGR);

    // Count objects by type
    int circleCount = 0, squareCount = 0, rectangleCount = 0;
    for (const auto& obj : objects) {
        if (obj.shape == "Circle") circleCount++;
        else if (obj.shape == "Square") squareCount++;
        else if (obj.shape == "Rectangle") rectangleCount++;
    }

    // Draw ROI rectangle if selected
    if (roiSelected && selectedROI.width > 0 && selectedROI.height > 0) {
        cv::rectangle(display, selectedROI, cv::Scalar(255, 0, 255), 2);
        cv::putText(display, "ROI", cv::Point(selectedROI.x, selectedROI.y - 10),
            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 0, 255), 2);
    }

    // Draw detected objects with different colors for each type
    for (size_t i = 0; i < objects.size(); i++) {
        const auto& obj = objects[i];
        cv::Point center(obj.center.x, obj.center.y);

        // Choose color based on object type
        cv::Scalar color;
        if (obj.shape == "Circle") color = cv::Scalar(0, 255, 0);      // Green
        else if (obj.shape == "Square") color = cv::Scalar(255, 0, 0);  // Blue  
        else if (obj.shape == "Rectangle") color = cv::Scalar(0, 0, 255); // Red
        else color = cv::Scalar(128, 128, 128); // Gray for unknown

        // Draw center point
        cv::circle(display, center, 10, color, -1);

        // Draw label
        std::string label = std::to_string(i + 1) + ": " + obj.shape;
        cv::putText(display, label, cv::Point(center.x + 15, center.y),
            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
    }

    // Show detailed status with counts by type
    std::string status = "Total: " + std::to_string(objects.size()) +
        " (C:" + std::to_string(circleCount) +
        " S:" + std::to_string(squareCount) +
        " R:" + std::to_string(rectangleCount) + ")";
    cv::putText(display, status, cv::Point(10, 30),
        cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 0), 2);

    if (roiSelected) {
        cv::putText(display, "Using ROI", cv::Point(10, 60),
            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 0, 255), 2);
    }

    // Save to file
    char filename[100];
    if (isFinalResult) {
        sprintf_s(filename, sizeof(filename), "detection_final_result.jpg");
        cv::putText(display, "FINAL RESULT", cv::Point(10, 90),
            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 0, 0), 2);
    }
    else {
        sprintf_s(filename, sizeof(filename), "detection_attempt_%d.jpg", attemptNumber);
    }

    if (cv::imwrite(filename, display)) {
        printf("Detection result saved to: %s\n", filename);
    }
    else {
        printf("Failed to save detection result\n");
    }
}

// Perform object detection multiple times and return best result
std::vector<SimpleObject> performObjectDetection(IColorFrameReader* pColorReader, int attempts = 3) {
    std::vector<SimpleObject> bestResult;
    cv::Mat bestImage;

    printf("*** PERFORMING OBJECT DETECTION (%d attempts) ***\n", attempts);

    // ROI selection on first call
    if (!isSelectingROI && !roiSelected) {
        isSelectingROI = true;
        printf("*** FIRST TIME OBJECT DETECTION - ROI SELECTION ***\n");

        // Get a frame for ROI selection
        IColorFrame* pColorFrame = nullptr;
        cv::Mat colorMat;

        for (int frameAttempt = 0; frameAttempt < 20; frameAttempt++) {
            HRESULT hr = pColorReader->AcquireLatestFrame(&pColorFrame);
            if (SUCCEEDED(hr) && pColorFrame) {
                processColorFrame(pColorFrame, colorMat);
                SafeRelease(pColorFrame);
                break;
            }
            Sleep(100);
        }

        if (!colorMat.empty()) {
            //roiSelected = selectROI(colorMat);
            roiSelected = true;
            printf("ROI selected: x=%d, y=%d, width=%d, height=%d\n",
                selectedROI.x, selectedROI.y, selectedROI.width, selectedROI.height);
            printf("ROI selection completed. Selected: %s\n", roiSelected ? "Yes" : "No (using full image)");
        }
        else {
            printf("Failed to get frame for ROI selection, using full image\n");
        }

        isSelectingROI = false;
    }

    for (int attempt = 0; attempt < attempts; attempt++) {
        printf("Object detection attempt %d/%d...\n", attempt + 1, attempts);

        // Try to get a color frame
        IColorFrame* pColorFrame = nullptr;
        cv::Mat colorMat;

        // Wait a bit and try to get a frame
        for (int frameAttempt = 0; frameAttempt < 10; frameAttempt++) {
            HRESULT hr = pColorReader->AcquireLatestFrame(&pColorFrame);
            if (SUCCEEDED(hr) && pColorFrame) {
                processColorFrame(pColorFrame, colorMat);
                SafeRelease(pColorFrame);
                break;
            }
            Sleep(100);
        }

        if (!colorMat.empty()) {
            std::vector<SimpleObject> objects = detectObjects(colorMat);

            // Keep the result with the most objects (not just if >= 2)
            if (objects.size() > bestResult.size()) {
                bestResult = objects;
                bestImage = colorMat.clone();
            }

            // Show current attempt results with type breakdown
            int circles = 0, squares = 0, rectangles = 0;
            for (const auto& obj : objects) {
                if (obj.shape == "Circle") circles++;
                else if (obj.shape == "Square") squares++;
                else if (obj.shape == "Rectangle") rectangles++;
            }

            printf("Attempt %d: Found %zu objects (Circles: %d, Squares: %d, Rectangles: %d)\n",
                attempt + 1, objects.size(), circles, squares, rectangles);
            saveDetectionResults(colorMat, objects, attempt + 1, false);
        }

        if (attempt < attempts - 1) {
            Sleep(300);
        }
    }

    // Save final best result
    if (!bestImage.empty()) {
        printf("*** FINAL BEST RESULT ***\n");
        saveDetectionResults(bestImage, bestResult, 0, true);
    }

    // Show final statistics
    int finalCircles = 0, finalSquares = 0, finalRectangles = 0;
    for (const auto& obj : bestResult) {
        if (obj.shape == "Circle") finalCircles++;
        else if (obj.shape == "Square") finalSquares++;
        else if (obj.shape == "Rectangle") finalRectangles++;
    }

    printf("Object detection complete. Found %zu total objects:\n", bestResult.size());
    printf("  - Circles: %d\n", finalCircles);
    printf("  - Squares: %d\n", finalSquares);
    printf("  - Rectangles: %d\n", finalRectangles);

    for (size_t i = 0; i < bestResult.size(); i++) {
        printf("  Object %zu: %s at (%.1f, %.1f)\n", i + 1,
            bestResult[i].shape.c_str(),
            bestResult[i].center.x,
            bestResult[i].center.y);
    }

    printf("Detection images saved. Continuing depth detection...\n");
    return bestResult;
}


// Helper function to find if object already exists in tracker
TrackedObject* findExistingObject(const cv::Point2f& center, char type) {
    for (auto& tracked : globalObjectTracker) {
        if (tracked.type == type) {
            float distance = sqrt(pow(tracked.center.x - center.x, 2) +
                pow(tracked.center.y - center.y, 2));
            if (distance <= OBJECT_MATCH_THRESHOLD) {
                return &tracked;  // Found existing object
            }
        }
    }
    return NULL;  // No existing object found
}

void exportDepthToTxt(UINT16* depthData, int width, int height, const char* filename, const std::vector<SimpleObject>& objects) {
    const int TARGET_WIDTH = 100;
    const int TARGET_HEIGHT = 100;

    // ROI constants
    const float ROI_X_START_PERCENT = 0.214f;
    const float ROI_Y_START_PERCENT = 0.205f;
    const float ROI_WIDTH_PERCENT = 0.632f;
    const float ROI_HEIGHT_PERCENT = 0.51f;

    // Calculate ROI
    int orig_x_min = (int)(width * ROI_X_START_PERCENT);
    int orig_y_min = (int)(height * ROI_Y_START_PERCENT);
    int orig_x_max = orig_x_min + (int)(width * ROI_WIDTH_PERCENT);
    int orig_y_max = orig_y_min + (int)(height * ROI_HEIGHT_PERCENT);

    // Clamp to image boundaries
    orig_x_min = std::max(0, orig_x_min);
    orig_y_min = std::max(0, orig_y_min);
    orig_x_max = std::min(width - 1, orig_x_max);
    orig_y_max = std::min(height - 1, orig_y_max);

    int cropped_width = orig_x_max - orig_x_min + 1;
    int cropped_height = orig_y_max - orig_y_min + 1;

    printf("Original image: %dx%d, ROI: [%d,%d] to [%d,%d]\n",
        width, height, orig_x_min, orig_y_min, orig_x_max, orig_y_max);

    // Open file
    FILE* file = NULL;
    errno_t err = fopen_s(&file, filename, "wb");
    if (err != 0 || !file) {
        printf("Failed to open file: %s\n", filename);
        return;
    }

    // Process depth data (same as before)
    UINT16* targetDepth = (UINT16*)malloc(TARGET_WIDTH * TARGET_HEIGHT * sizeof(UINT16));
    if (!targetDepth) {
        printf("Failed to allocate memory\n");
        fclose(file);
        return;
    }

    const float roi_scale_x = (float)cropped_width / TARGET_WIDTH;
    const float roi_scale_y = (float)cropped_height / TARGET_HEIGHT;

    // Fill 100x100 grid with depth data
    for (int target_y = 0; target_y < TARGET_HEIGHT; target_y++) {
        for (int target_x = 0; target_x < TARGET_WIDTH; target_x++) {
            int start_x = orig_x_min + (int)(target_x * roi_scale_x);
            int end_x = orig_x_min + (int)((target_x + 1) * roi_scale_x);
            int start_y = orig_y_min + (int)(target_y * roi_scale_y);
            int end_y = orig_y_min + (int)((target_y + 1) * roi_scale_y);

            end_x = std::min(end_x, orig_x_max);
            end_y = std::min(end_y, orig_y_max);

            unsigned int sum = 0;
            int valid_count = 0;

            for (int src_y = start_y; src_y <= end_y; src_y++) {
                for (int src_x = start_x; src_x <= end_x; src_x++) {
                    int idx = src_y * width + src_x;
                    UINT16 depth = depthData[idx];
                    if (depth >= DEPTH_MIN && depth <= DEPTH_MAX) {
                        sum += depth;
                        valid_count++;
                    }
                }
            }

            int target_idx = target_y * TARGET_WIDTH + target_x;
            targetDepth[target_idx] = (valid_count > 0) ? (UINT16)(sum / valid_count) : 0;
        }
    }

    // Transform depth values
    float* transformedDepth = (float*)malloc(TARGET_WIDTH * TARGET_HEIGHT * sizeof(float));
    if (!transformedDepth) {
        printf("Failed to allocate transformed depth memory\n");
        free(targetDepth);
        fclose(file);
        return;
    }

    //transform depth
    for (int i = 0; i < TARGET_WIDTH * TARGET_HEIGHT; i++) {
        UINT16 originalDepth = targetDepth[i];
        if (originalDepth > 770) {
            transformedDepth[i] = -20;
        }
        else if (originalDepth < 650) {
            transformedDepth[i] = 800;
        }
        else {
            // Scale from [500, 770] to [800, -20] (inverted)
            transformedDepth[i] = 800 - ((float)(originalDepth - 650) / (770 - 650)) * (800 - (-20));
        }
    }

    // Write depth data to file
    fwrite(transformedDepth, sizeof(float), TARGET_WIDTH * TARGET_HEIGHT, file);
    free(transformedDepth);
    free(targetDepth);

    // === SIMPLE OBJECT TRACKING ===

    printf("=== Object Tracking ===\n");
    printf("Objects detected: %d\n", (int)objects.size());

    int newObjectsWritten = 0;

    // Process each detected object
    for (const auto& detectedObj : objects) {
        char objType;
        uint8_t fileOffset;

        // Get object type and file offset
        if (detectedObj.shape == "Rectangle") {
            objType = 'R';
            fileOffset = 0x00;
        }
        else if (detectedObj.shape == "Square") {
            objType = 'S';
            fileOffset = 0x01;
        }
        else if (detectedObj.shape == "Circle") {
            objType = 'C';
            fileOffset = 0x10;
        }
        else {
            printf("Unknown shape: %s, skipping\n", detectedObj.shape.c_str());
            continue;
        }

        if (detectedObj.center.x <= 98 && detectedObj.center.y <= 98) {
            cv::Point2f center = detectedObj.center;
            printf("Processing %c at (%.1f, %.1f)\n", objType, center.x, center.y);

            // Check if object already exists in tracker
            TrackedObject* existing = findExistingObject(center, objType);

            if (existing != nullptr) {
                // Object already exists
                existing->center = center;  // Update position
                if (existing->writtenToFile) {
                    printf("  -> Already written to file, skipping\n");
                }
                else {
                    printf("  -> ERROR: Object exists but not written? This shouldn't happen!\n");
                }
            }
            else {
                // This is a NEW object
                printf("  -> NEW object detected!\n");

                // Add to global tracker
                globalObjectTracker.emplace_back(center, objType);
                center.x = 100 - center.x;
                // Write to file
                fwrite(&fileOffset, sizeof(uint8_t), 1, file);
                fwrite(&center.x, sizeof(float), 1, file);
                fwrite(&center.y, sizeof(float), 1, file);

                // Mark as written
                globalObjectTracker.back().writtenToFile = true;
                newObjectsWritten++;

                printf("  -> Written to file with offset 0x%02X\n", fileOffset);
            }
        }
    }

    printf("New objects written to file: %d\n", newObjectsWritten);
    printf("Total tracked objects: %d\n", (int)globalObjectTracker.size());

    // Debug: Show all tracked objects
    printf("All tracked objects:\n");
    for (size_t i = 0; i < globalObjectTracker.size(); i++) {
        const auto& obj = globalObjectTracker[i];
        printf("  %d: %c at (%.1f, %.1f) - %s\n",
            (int)i, obj.type, obj.center.x, obj.center.y,
            obj.writtenToFile ? "written" : "NOT written");
    }

    fclose(file);

    system(SCP);
}



void analyzeRawDepthData(UINT16* depthData, int width, int height, const char* stage) {
    int validCount = 0;
    int zeroCount = 0;
    int tooCloseCount = 0;
    int tooFarCount = 0;
    float minDepth = 65535;
    float maxDepth = 0;
    unsigned long long sum = 0;

    for (int i = 0; i < width * height; i++) {
        float depth = depthData[i];

        if (depth == 0) {
            zeroCount++;
        }
        else if (depth < DEPTH_MIN) {
            tooCloseCount++;
            //if (depth < minDepth) minDepth = depth;
            //if (depth > maxDepth) maxDepth = depth;
        }
        else if (depth > DEPTH_MAX) {
            tooFarCount++;
            //if (depth < minDepth) minDepth = depth;
            //if (depth > maxDepth) maxDepth = depth;
        }
        else {
            validCount++;
            sum += depth;
            if (depth < minDepth) minDepth = depth;
            if (depth > maxDepth) maxDepth = depth;
        }
    }

    printf("\n=== DEPTH ANALYSIS (%s) ===\n", stage);
    printf("Total pixels: %d\n", width * height);
    printf("Zero values: %d (%.1f%%)\n", zeroCount, zeroCount * 100.0f / (width * height));
    printf("Too close (<%d): %d (%.1f%%)\n", DEPTH_MIN, tooCloseCount, tooCloseCount * 100.0f / (width * height));
    printf("Valid range: %d (%.1f%%)\n", validCount, validCount * 100.0f / (width * height));
    printf("Too far (>%d): %d (%.1f%%)\n", DEPTH_MAX, tooFarCount, tooFarCount * 100.0f / (width * height));

    if (minDepth != 65535) {
        printf("Depth range: %0.2f to %0.2f mm\n", minDepth, maxDepth);
        if (validCount > 0) {
            printf("Average valid depth: %.1f mm\n", (float)sum / validCount);
        }
    }
    else {
        printf("No depth values found!\n");
    }
    printf("=== END ANALYSIS ===\n\n");
}

void extractDepthROI(UINT16* sourceDepth, int sourceWidth, int sourceHeight,
    UINT16* roiDepth, int roiX, int roiY, int roiWidth, int roiHeight) {
    // Validate ROI bounds
    if (roiX < 0 || roiY < 0 ||
        roiX + roiWidth > sourceWidth ||
        roiY + roiHeight > sourceHeight) {
        printf("Error: ROI bounds exceed source frame dimensions\n");
        printf("Source: %dx%d, ROI: %d,%d %dx%d\n",
            sourceWidth, sourceHeight, roiX, roiY, roiWidth, roiHeight);
        return;
    }

    printf("Extracting ROI: [%d,%d] %dx%d from %dx%d frame\n",
        roiX, roiY, roiWidth, roiHeight, sourceWidth, sourceHeight);

    // Copy ROI data row by row
    for (int y = 0; y < roiHeight; y++) {
        int sourceRowStart = (roiY + y) * sourceWidth + roiX;
        int roiRowStart = y * roiWidth;

        memcpy(&roiDepth[roiRowStart], &sourceDepth[sourceRowStart], roiWidth * sizeof(UINT16));
    }
}

// Alternative method using manual swapping
void mirrorHorizontalManual(uint16_t* data, int rows, int cols) {
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols / 2; col++) {
            int leftIndex = row * cols + col;
            int rightIndex = row * cols + (cols - 1 - col);

            // Swap elements
            uint16_t temp = data[leftIndex];
            data[leftIndex] = data[rightIndex];
            data[rightIndex] = temp;
        }
    }
}


int main() {
    // Initialize Kinect sensor
    IKinectSensor* sensor = NULL;
    HRESULT hr = GetDefaultKinectSensor(&sensor);
    CheckResult(hr, "getting default sensor");

    hr = sensor->Open();
    CheckResult(hr, "opening sensor");

    // Get depth frame source
    IDepthFrameSource* depthSource = NULL;
    hr = sensor->get_DepthFrameSource(&depthSource);
    CheckResult(hr, "getting depth frame source");

    UINT16 minReliableDistance, maxReliableDistance;
    hr = depthSource->get_DepthMinReliableDistance(&minReliableDistance);
    hr = depthSource->get_DepthMaxReliableDistance(&maxReliableDistance);

    printf("Kinect reliable depth range: %d to %d mm\n", minReliableDistance, maxReliableDistance);


    // Get depth frame description
    IFrameDescription* depthDescription = NULL;
    hr = depthSource->get_FrameDescription(&depthDescription);
    CheckResult(hr, "getting frame description");

    int width = 0;
    int height = 0;
    depthDescription->get_Width(&width);
    depthDescription->get_Height(&height);
    depthDescription->Release();
    printf("Depth frame size: %d x %d\n", width, height);

    // Open depth reader
    IDepthFrameReader* depthReader = NULL;
    hr = depthSource->OpenReader(&depthReader);
    CheckResult(hr, "opening depth reader");
    depthSource->Release();

    // Get color frame source and reader for object detection
    IColorFrameSource* pColorSource = nullptr;
    IColorFrameReader* pColorReader = nullptr;

    hr = sensor->get_ColorFrameSource(&pColorSource);
    if (SUCCEEDED(hr)) {
        hr = pColorSource->OpenReader(&pColorReader);
    }

    if (FAILED(hr)) {
        printf("Warning: Failed to initialize color reader for object detection!\n");
        pColorReader = nullptr;
    }
    else {
        printf("Color reader initialized for object detection\n");
    }

    // Allocate buffers for depth data processing
    UINT16* rawDepth = new UINT16[width * height];
    UINT16* processedDepth = new UINT16[width * height];
    UINT16* previousDepth = new UINT16[width * height];
    UINT16* tempBuffer = new UINT16[width * height];
    UINT16* lastSentDepth = new UINT16[width * height];


    const int ROI_WIDTH = 340; //370  // Your desired ROI width
    const int ROI_HEIGHT = 240;//270  // Your desired ROI height
    const int ROI_X = 80;// 50 //(width - ROI_WIDTH) / 2;   // Center horizontally
    const int ROI_Y = 80;// 90 //(height - ROI_HEIGHT) / 2; // Center vertically


    // Initialize buffers with zeros
    memset(previousDepth, 0, width * height * sizeof(UINT16));
    memset(lastSentDepth, 0, width * height * sizeof(UINT16));

    UINT16* roiProcessed = new UINT16[ROI_WIDTH * ROI_HEIGHT];
    UINT16* roiPrevious = new UINT16[ROI_WIDTH * ROI_HEIGHT];
    memset(roiProcessed, 0, ROI_WIDTH * ROI_HEIGHT * sizeof(UINT16));

    int frameCount = 0;
    bool isFirstFrame = true;
    bool terrainChanging = false;
    DWORD lastChangeTime = 0;
    DWORD lastUpdateTime = 0;
    DWORD lastExportTime = 0;
    DWORD lastPrintTime = 0;

    printf("Starting depth frame processing with object detection. Press Ctrl+C to exit.\n");

    // Main processing loop
    while (true) {
        IDepthFrame* depthFrame = NULL;
        DWORD currentTime = GetTickCount();

        // Try to get a new frame
        hr = depthReader->AcquireLatestFrame(&depthFrame);
        if (SUCCEEDED(hr)) {
            // Copy depth data to our buffer
            hr = depthFrame->CopyFrameDataToArray(width * height, rawDepth);
            CheckResult(hr, "copying frame data");
            depthFrame->Release();

            frameCount++;
            printf("\n----- Processing Frame #%d -----\n", frameCount);
            mirrorHorizontalManual(rawDepth, height, width);

            // --- Processing Pipeline ---
            analyzeRawDepthData(rawDepth, width, height, "RAW from Kinect");

            // 1. Apply median filter to reduce noise
            applyMedianFilter(rawDepth, tempBuffer, width, height);

            // 2. Fill small holes
            fillHoles(tempBuffer, width, height);

            // 3. Apply bilateral filter for edge-preserving smoothing
            applyBilateralFilter(tempBuffer, processedDepth, width, height);

            if (currentTime - lastPrintTime >= PRINT_INTERVAL) {
                // Show simple visualization (^ for close objects, space for far objects)
                lastPrintTime = currentTime;
                visualizeSimpleDepthMap(processedDepth, width, height, 5); // Using step=5 for a more compact display
            }

            // Check for changes if not the first frame
            if (!isFirstFrame) {
                double changePercentage;
                extractDepthROI(processedDepth, width, height, roiProcessed, ROI_X, ROI_Y, ROI_WIDTH, ROI_HEIGHT);
                bool significantChange = detectChanges(
                    roiProcessed, roiPrevious, ROI_WIDTH, ROI_HEIGHT, &changePercentage);

                printf("Change analysis: %.2f%% pixels changed\n", changePercentage);

                // State transition logic
                if (significantChange) {
                    // Terrain is changing
                    if (!terrainChanging) {
                        printf("Starting to detect terrain changes...\n");
                        terrainChanging = true;
                    }
                    lastChangeTime = currentTime;
                }
                else if (terrainChanging) {
                    // Check if terrain has stabilized (no changes for STABILITY_TIMEOUT)
                    DWORD timeSinceChange = currentTime - lastChangeTime;
                    printf("Terrain stabilizing... Time since last change: %d ms\n", timeSinceChange);

                    if (timeSinceChange >= STABILITY_TIMEOUT) {
                        // Terrain has stabilized, perform object detection and send update
                        printf("\n*** TERRAIN HAS STABILIZED - PERFORMING OBJECT DETECTION ***\n");
                        terrainChanging = false;

                        // Perform object detection
                        std::vector<SimpleObject> detectedObjects;
                        if (pColorReader != nullptr) {
                            detectedObjects = performObjectDetection(pColorReader, 3); // Try 3 times
                        }
                        else {
                            printf("Warning: Color reader not available, skipping object detection\n");
                        }

                        // Get current timestamp for filename
                        time_t now;
                        struct tm timeinfo;
                        char timestamp[80];

                        time(&now);
                        localtime_s(&timeinfo, &now);
                        strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &timeinfo);

                        // Export depth data with object coordinates
                        char filename[100];
                        sprintf_s(filename, sizeof(filename), FILEPATH);
                        exportDepthToTxt(processedDepth, width, height, filename, detectedObjects);

                        // Remember what we sent
                        memcpy(lastSentDepth, processedDepth, width * height * sizeof(UINT16));
                        lastUpdateTime = currentTime;
                    }
                }
                // Check if there were no significant changes and we haven't sent an update yet
                if (!significantChange && !terrainChanging && isFirstFrame) {
                    // First stable frame, send initial update
                    printf("\n*** SENDING INITIAL TERRAIN MAP ***\n");

                    // Perform object detection for initial frame
                    std::vector<SimpleObject> detectedObjects;
                    if (pColorReader != nullptr) {
                        detectedObjects = performObjectDetection(pColorReader, 3); // Try 3 times
                    }

                    char filename[100];
                    sprintf_s(filename, sizeof(filename), FILEPATH);
                    exportDepthToTxt(processedDepth, width, height, filename, detectedObjects);

                    memcpy(lastSentDepth, processedDepth, width * height * sizeof(UINT16));
                    lastUpdateTime = currentTime;
                }
            }
            else {
                isFirstFrame = false;
                printf("First processed frame captured\n");

                // Send initial terrain map with object detection
                printf("\n*** SENDING INITIAL TERRAIN MAP ***\n");

                // Perform object detection for initial frame
                std::vector<SimpleObject> detectedObjects;
                if (pColorReader != nullptr) {
                    detectedObjects = performObjectDetection(pColorReader, 3); // Try 3 times
                }

                // Export initial depth data with object coordinates
                char filename[100];
                sprintf_s(filename, sizeof(filename), FILEPATH);
                exportDepthToTxt(processedDepth, width, height, filename, detectedObjects);

                memcpy(lastSentDepth, processedDepth, width * height * sizeof(UINT16));
                lastUpdateTime = currentTime;
            }

            // Update previous frame with current processed frame for next comparison
            memcpy(previousDepth, processedDepth, width * height * sizeof(UINT16));
            memcpy(roiPrevious, roiProcessed, ROI_WIDTH * ROI_HEIGHT * sizeof(UINT16));
        }
        else {
            // Failed to acquire frame, wait a bit and try again
            Sleep(10);
            continue;
        }

        // Control capture rate
        Sleep(100);  // Process up to 10 frames per second
    }

    // Cleanup - only at program exit, no OpenCV windows to close
    delete[] rawDepth;
    delete[] processedDepth;
    delete[] previousDepth;
    delete[] tempBuffer;
    delete[] lastSentDepth;
    delete[] roiPrevious;
    delete[] roiProcessed;

    depthReader->Release();
    SafeRelease(pColorReader);
    SafeRelease(pColorSource);
    sensor->Close();
    sensor->Release();

    return 0;
}