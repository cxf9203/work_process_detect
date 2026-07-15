#ifdef USE_OPENVINO
    #include "yolov8_openvino.h"
#else
    #include "yolov8_tensorrt.h"
#endif

void YoloV8::drawObjectLabels(cv::Mat &image, const std::vector<Object> &objects, unsigned int scale)
{
    // 清空std::vector classCount
    classCount[0] = 0;
    classCount[1] = 0;
    classCount[2] = 0;
    actionFlag = {false, false, false, false, false}; // 初始化动作标志

    // 绘制检测区域ROI
    if (useROI && !detectionROI.empty())
    {
        cv::Mat overlay = image.clone();
        cv::rectangle(overlay, detectionROI, roiColor, roiLineWidth);
        cv::addWeighted(image, roiOpacity, overlay, 1.0f - roiOpacity, 0, image);
    }

    // If segmentation information is present, start with that
    if (!objects.empty() && !objects[0].boxMask.empty())
    {
        std::cout << "have mask" << std::endl;
        for (const auto &object : objects)
        {
            // Choose the color
            int colorIndex = object.label % COLOR_LIST.size();
            cv::Scalar color = cv::Scalar(COLOR_LIST[colorIndex][0], COLOR_LIST[colorIndex][1], COLOR_LIST[colorIndex][2]);
            std::cout << "have mask processed" << std::endl;
        }
    }

    // Bounding boxes and annotations
    for (auto &object : objects)
    {
        // 检查对象是否在ROI区域内
        if (useROI && !detectionROI.empty())
        {
            // 计算对象中心点
            cv::Point center(object.rect.x + object.rect.width / 2, object.rect.y + object.rect.height / 2);
            // 检查中心点是否在ROI区域内
            if (!detectionROI.contains(center))
            {
                continue; // 跳过不在ROI区域内的对象
            }
        }

        // Choose the color
        int colorIndex = object.label % COLOR_LIST.size();
        cv::Scalar color = cv::Scalar(COLOR_LIST[colorIndex][0], COLOR_LIST[colorIndex][1], COLOR_LIST[colorIndex][2]);
        float meanColor = cv::mean(color)[0];
        cv::Scalar txtColor = meanColor > 0.5 ? cv::Scalar(0, 0, 0) : cv::Scalar(255, 255, 255);

        const auto &rect = object.rect;

        std::string label_name = CLASS_NAMES[object.label];
        std::cout << "label_name: " << label_name << std::endl;
        std::cout << "label_id: " << object.label << std::endl;
        std::cout << "label_rect: " << object.rect << std::endl;
        std::cout << "probability: " << object.probability * 100 << std::endl;
        // 统计各对象个数和动作标志
        switch (object.label)
        {
            case 0: classCount[0]++; break;
            case 1: classCount[1]++; break;
            case 2: classCount[2]++; break;
            case 4: actionFlag[0] = true; break; // 左上螺丝
            case 6: actionFlag[1] = true; break; // 右上螺丝
            case 3: actionFlag[2] = true; break; // 左下螺丝
            case 5: actionFlag[3] = true; break; // 右下螺丝
            case 7: actionFlag[4] = true; break; // 放置齿轮
        }
        // Draw rectangles and text
        char text[256];
        sprintf(text, "%s %.1f%%", CLASS_NAMES[object.label].c_str(), object.probability * 100);

        int baseLine = 0;
        cv::Size labelSize = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.35 * scale, scale, &baseLine);
        cv::Scalar txt_bk_color = color * 0.7 * 255;

        int x = object.rect.x;
        int y = object.rect.y + 1;

        cv::rectangle(image, rect, color * 255, scale + 1);
        cv::rectangle(image, cv::Rect(cv::Point(x, y), cv::Size(labelSize.width, labelSize.height + baseLine)), txt_bk_color, -1);
        cv::putText(image, text, cv::Point(x, y + labelSize.height), cv::FONT_HERSHEY_SIMPLEX, 0.35 * scale, txtColor, scale);

        // Pose estimation
        if (!object.kps.empty())
        {
            auto &kps = object.kps;
            for (int k = 0; k < NUM_KPS + 2; k++)
            {
                if (k < NUM_KPS)
                {
                    int kpsX = std::round(kps[k * 3]);
                    int kpsY = std::round(kps[k * 3 + 1]);
                    float kpsS = kps[k * 3 + 2];
                    if (kpsS > KPS_THRESHOLD)
                    {
                        cv::Scalar kpsColor = cv::Scalar(KPS_COLORS[k][0], KPS_COLORS[k][1], KPS_COLORS[k][2]);
                        cv::circle(image, {kpsX, kpsY}, 5, kpsColor, -1);
                    }
                }
                auto &ske = SKELETON[k];
                int pos1X = std::round(kps[(ske[0] - 1) * 3]);
                int pos1Y = std::round(kps[(ske[0] - 1) * 3 + 1]);
                int pos2X = std::round(kps[(ske[1] - 1) * 3]);
                int pos2Y = std::round(kps[(ske[1] - 1) * 3 + 1]);
                float pos1S = kps[(ske[0] - 1) * 3 + 2];
                float pos2S = kps[(ske[1] - 1) * 3 + 2];
                if (pos1S > KPS_THRESHOLD && pos2S > KPS_THRESHOLD)
                {
                    cv::Scalar limbColor = cv::Scalar(LIMB_COLORS[k][0], LIMB_COLORS[k][1], LIMB_COLORS[k][2]);
                    cv::line(image, {pos1X, pos1Y}, {pos2X, pos2Y}, limbColor, 2);
                }
            }
        }
    }
}

std::vector<int> YoloV8::getclassnumer()
{ // 获取每个标签类的数量
    return classCount;
}

std::vector<bool> YoloV8::getActionFlag()
{
    return actionFlag;
}

// 启用或禁用ROI检测
void YoloV8::enableROIDetection(bool enable)
{
    useROI = enable;
}

// 设置检测区域
void YoloV8::setDetectionROI(const cv::Rect &roi)
{
    detectionROI = roi;
}

// 设置ROI颜色
void YoloV8::setRoiColor(const QString color)
{
    QColor qColor(color);
    roiColor = cv::Scalar(qColor.blue(), qColor.green(), qColor.red());
}

// 设置ROI透明度
void YoloV8::setRoiOpacity(float opacity)
{
    roiOpacity = opacity;
}

// 设置ROI线宽
void YoloV8::setRoiLineWidth(int lineWidth)
{
    roiLineWidth = lineWidth;
}
