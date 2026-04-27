#define _CRT_SECURE_NO_WARNINGS
#include "yolov8.h"
#include <opencv2/cudaimgproc.hpp>

YoloV8::YoloV8(const std::string &onnxModelPath, const YoloV8Config &config)
    : PROBABILITY_THRESHOLD(config.probabilityThreshold),
      NMS_THRESHOLD(config.nmsThreshold),
      TOP_K(config.topK),
      SEG_CHANNELS(config.segChannels),
      SEG_H(config.segH),
      SEG_W(config.segW),
      SEGMENTATION_THRESHOLD(config.segmentationThreshold),
      CLASS_NAMES(config.classNames),
      NUM_KPS(config.numKPS),
      KPS_THRESHOLD(config.kpsThreshold)
{
    // Specify options for GPU inference
    Options options;
    options.optBatchSize = 1;
    options.maxBatchSize = 1;

    options.precision = config.precision;
    options.calibrationDataDirectoryPath = config.calibrationDataDirectory;

    if (options.precision == Precision::INT8)
    {
        if (options.calibrationDataDirectoryPath.empty())
        {
            throw std::runtime_error("Error: Must supply calibration data path for INT8 calibration");
        }
    }

    // Create our TensorRT inference engine
    m_trtEngine = std::make_unique<Engine<float>>(options);

    // Build the onnx model into a TensorRT engine file, cache the file to disk, and then load the TensorRT engine file into memory.
    // If the engine file already exists on disk, this function will not rebuild but only load into memory.
    // The engine file is rebuilt any time the above Options are changed.
    auto succ = m_trtEngine->buildLoadNetwork(onnxModelPath, SUB_VALS, DIV_VALS, NORMALIZE);
    if (!succ)
    {
        const std::string errMsg = "Error: Unable to build or load the TensorRT engine. "
                                   "Try increasing TensorRT log severity to kVERBOSE (in /libs/tensorrt-cpp-api/engine.cpp).";
        throw std::runtime_error(errMsg);
    }
}

std::vector<std::vector<cv::cuda::GpuMat>> YoloV8::preprocess(const cv::cuda::GpuMat &gpuImg)
{
    // Populate the input vectors
    const auto &inputDims = m_trtEngine->getInputDims();

    // Convert the image from BGR to RGB
    cv::cuda::GpuMat rgbMat;
    cv::cuda::cvtColor(gpuImg, rgbMat, cv::COLOR_BGR2RGB);

    auto resized = rgbMat;

    // Resize to the model expected input size while maintaining the aspect ratio with the use of padding
    if (resized.rows != inputDims[0].d[1] || resized.cols != inputDims[0].d[2])
    {
        // Only resize if not already the right size to avoid unecessary copy
        resized = Engine<float>::resizeKeepAspectRatioPadRightBottom(rgbMat, inputDims[0].d[1], inputDims[0].d[2]);
    }

    // Convert to format expected by our inference engine
    // The reason for the strange format is because it supports models with multiple inputs as well as batching
    // In our case though, the model only has a single input and we are using a batch size of 1.
    std::vector<cv::cuda::GpuMat> input{std::move(resized)};
    std::vector<std::vector<cv::cuda::GpuMat>> inputs{std::move(input)};

    // These params will be used in the post-processing stage
    m_imgHeight = rgbMat.rows;
    m_imgWidth = rgbMat.cols;
    m_ratio = 1.f / std::min(inputDims[0].d[2] / static_cast<float>(rgbMat.cols), inputDims[0].d[1] / static_cast<float>(rgbMat.rows));

    return inputs;
}

std::vector<Object> YoloV8::detectObjects(const cv::cuda::GpuMat &inputImageBGR)
{
    // Preprocess the input image
#ifdef ENABLE_BENCHMARKS
    static int numIts = 1;
    preciseStopwatch s1;
#endif
    const auto input = preprocess(inputImageBGR);
#ifdef ENABLE_BENCHMARKS
    static long long t1 = 0;
    t1 += s1.elapsedTime<long long, std::chrono::microseconds>();
    std::cout << "Avg Preprocess time: " << (t1 / numIts) / 1000.f << " ms" << std::endl;
#endif
    // Run inference using the TensorRT engine
#ifdef ENABLE_BENCHMARKS
    preciseStopwatch s2;
#endif
    std::vector<std::vector<std::vector<float>>> featureVectors;
    auto succ = m_trtEngine->runInference(input, featureVectors);
    if (!succ)
    {
        throw std::runtime_error("Error: Unable to run inference.");
    }
#ifdef ENABLE_BENCHMARKS
    static long long t2 = 0;
    t2 += s2.elapsedTime<long long, std::chrono::microseconds>();
    std::cout << "Avg Inference time: " << (t2 / numIts) / 1000.f << " ms" << std::endl;
    preciseStopwatch s3;
#endif
    // Check if our model does only object detection or also supports segmentation
    std::vector<Object> ret;
    const auto &numOutputs = m_trtEngine->getOutputDims().size();
    // std::cout << " numOutputs is " << numOutputs << std::endl;

    if (numOutputs == 1)
    {
        // Object detection or pose estimation
        // Since we have a batch size of 1 and only 1 output, we must convert the output from a 3D array to a 1D array.
        std::vector<float> featureVector;
        Engine<float>::transformOutput(featureVectors, featureVector);

        const auto &outputDims = m_trtEngine->getOutputDims();
        int numChannels = outputDims[outputDims.size() - 1].d[1];

        // TODO: Need to improve this to make it more generic (don't use magic number).
        // For now it works with Ultralytics pretrained models.
        if (numChannels == 56)
        {
            // Pose estimation
            ret = postProcessPose(featureVector);
        }

        else
        {
            // Object detection or classify
            ret = postProcessDetect(featureVector);
            // Itemindex = postProcessClassify(featureVector);
        }
    }
    else
    {
        // Segmentation
        // Since we have a batch size of 1 and 2 outputs, we must convert the output from a 3D array to a 2D array.
        std::cout << "support segmentation" << std::endl;
        std::vector<std::vector<float>> featureVector;
        Engine<float>::transformOutput(featureVectors, featureVector);
        std::cout << "run postProcessSegmentation" << std::endl;
        ret = postProcessSegmentation(featureVector);
        std::cout << "run here segment" << std::endl;
    }
#ifdef ENABLE_BENCHMARKS
    static long long t3 = 0;
    t3 += s3.elapsedTime<long long, std::chrono::microseconds>();
    std::cout << "Avg Postprocess time: " << (t3 / numIts++) / 1000.f << " ms\n"
              << std::endl;
#endif
    return ret;
}

std::vector<Object> YoloV8::detectObjects(const cv::Mat &inputImageBGR)
{
    // Upload the image to GPU memory
    cv::cuda::GpuMat gpuImg;
    gpuImg.upload(inputImageBGR);

    // Call detectObjects with the GPU image
    return detectObjects(gpuImg);
}

std::vector<Object> YoloV8::postProcessSegmentation(std::vector<std::vector<float>> &featureVectors)
{
    const auto &outputDims = m_trtEngine->getOutputDims();
    // 1 116 8400
    std::cout << outputDims[1].d[0] << "  " << outputDims[1].d[1] << "  " << outputDims[1].d[2] << std::endl;
    // 1 32 160 160
    std::cout << outputDims[0].d[1] << " " << outputDims[0].d[2] << outputDims[0].d[3] << std::endl;
    int numChannels = outputDims[1].d[1]; // 116
    int numAnchors = outputDims[1].d[2];  // 8400

    const auto numClasses = numChannels - SEG_CHANNELS - 4;
    std::cout << featureVectors[0].size() << " " << featureVectors[1].size() << std::endl;
    // Ensure the output lengths are correct
    if (featureVectors[1].size() != static_cast<size_t>(numChannels) * numAnchors)
    {
        throw std::logic_error("Output at index 1 has incorrect length");
    }

    if (featureVectors[0].size() != static_cast<size_t>(SEG_CHANNELS) * SEG_H * SEG_W)
    {
        throw std::logic_error("Output at index 0 has incorrect length");
    }

    cv::Mat output = cv::Mat(numChannels, numAnchors, CV_32F, featureVectors[1].data());
    output = output.t();

    cv::Mat protos = cv::Mat(SEG_CHANNELS, SEG_H * SEG_W, CV_32F, featureVectors[0].data());

    std::vector<int> labels;
    std::vector<float> scores;
    std::vector<cv::Rect> bboxes;
    std::vector<cv::Mat> maskConfs;
    std::vector<int> indices;

    // Object the bounding boxes and class labels
    for (int i = 0; i < numAnchors; i++)
    {
        auto rowPtr = output.row(i).ptr<float>();
        auto bboxesPtr = rowPtr;
        auto scoresPtr = rowPtr + 4;
        auto maskConfsPtr = rowPtr + 4 + numClasses;
        auto maxSPtr = std::max_element(scoresPtr, scoresPtr + numClasses);
        float score = *maxSPtr;
        if (score > PROBABILITY_THRESHOLD)
        {
            float x = *bboxesPtr++;
            float y = *bboxesPtr++;
            float w = *bboxesPtr++;
            float h = *bboxesPtr;

            float x0 = std::clamp((x - 0.5f * w) * m_ratio, 0.f, m_imgWidth);
            float y0 = std::clamp((y - 0.5f * h) * m_ratio, 0.f, m_imgHeight);
            float x1 = std::clamp((x + 0.5f * w) * m_ratio, 0.f, m_imgWidth);
            float y1 = std::clamp((y + 0.5f * h) * m_ratio, 0.f, m_imgHeight);

            int label = maxSPtr - scoresPtr;
            cv::Rect_<float> bbox;
            bbox.x = x0;
            bbox.y = y0;
            bbox.width = x1 - x0;
            bbox.height = y1 - y0;

            cv::Mat maskConf = cv::Mat(1, SEG_CHANNELS, CV_32F, maskConfsPtr);

            bboxes.push_back(bbox);
            labels.push_back(label);
            scores.push_back(score);
            maskConfs.push_back(maskConf);
        }
    }
    std::cout << "finish Object the bounding boxes and class labels " << std::endl;
    // Require OpenCV 4.7 for this function
    cv::dnn::NMSBoxesBatched(bboxes, scores, labels, PROBABILITY_THRESHOLD, NMS_THRESHOLD, indices);

    // Obtain the segmentation masks
    cv::Mat masks;
    std::vector<Object> objs;
    int cnt = 0;
    for (auto &i : indices)
    {
        if (cnt >= TOP_K)
        {
            break;
        }
        cv::Rect tmp = bboxes[i];
        Object obj;
        obj.label = labels[i];
        obj.rect = tmp;
        obj.probability = scores[i];
        masks.push_back(maskConfs[i]);
        objs.push_back(obj);
        cnt += 1;
    }
    std::cout << "finish Object the bounding boxes and class labels " << std::endl;
    // Convert segmentation mask to original image
    if (!masks.empty())
    {
        cv::Mat matmulRes = (masks * protos).t();
        cv::Mat maskMat = matmulRes.reshape(indices.size(), {SEG_W, SEG_H});

        std::vector<cv::Mat> maskChannels;
        cv::split(maskMat, maskChannels);
        const auto inputDims = m_trtEngine->getInputDims();

        cv::Rect roi;
        if (m_imgHeight > m_imgWidth)
        {
            roi = cv::Rect(0, 0, SEG_W * m_imgWidth / m_imgHeight, SEG_H);
        }
        else
        {
            roi = cv::Rect(0, 0, SEG_W, SEG_H * m_imgHeight / m_imgWidth);
        }
        std::cout << "Convert segmentation mask to original image" << std::endl;
        try
        {
            for (size_t i = 0; i < indices.size(); i++)
            {
                cv::Mat dest, mask;
                cv::exp(-maskChannels[i], dest);
                dest = 1.0 / (1.0 + dest);
                dest = dest(roi);
                std::cout << "resize mask" << std::endl;
                cv::resize(dest, mask, cv::Size(static_cast<int>(m_imgWidth), static_cast<int>(m_imgHeight)), cv::INTER_LINEAR);
                std::cout << "resize mask2" << std::endl;
                objs[i].boxMask = mask(objs[i].rect) > SEGMENTATION_THRESHOLD;
                std::cout << "resize mask SEGMENTATION_THRESHOLD" << std::endl;
            }
        }
        catch (const char *msg)
        {
            std::cerr << msg << std::endl; // 捕获并处理异常
            std::cout << "error occur" << std::endl;
        }
    }
    std::cout << "postProcessSegmentation finish" << std::endl;
    return objs;
}

std::vector<Object> YoloV8::postProcessPose(std::vector<float> &featureVector)
{
    const auto &outputDims = m_trtEngine->getOutputDims();
    auto numChannels = outputDims[0].d[1];
    auto numAnchors = outputDims[0].d[2];

    std::vector<cv::Rect> bboxes;
    std::vector<float> scores;
    std::vector<int> labels;
    std::vector<int> indices;
    std::vector<std::vector<float>> kpss;

    cv::Mat output = cv::Mat(numChannels, numAnchors, CV_32F, featureVector.data());
    output = output.t();

    // Get all the YOLO proposals
    for (int i = 0; i < numAnchors; i++)
    {
        auto rowPtr = output.row(i).ptr<float>();
        auto bboxesPtr = rowPtr;
        auto scoresPtr = rowPtr + 4;
        auto kps_ptr = rowPtr + 5;
        float score = *scoresPtr;
        if (score > PROBABILITY_THRESHOLD)
        {
            float x = *bboxesPtr++;
            float y = *bboxesPtr++;
            float w = *bboxesPtr++;
            float h = *bboxesPtr;

            float x0 = std::clamp((x - 0.5f * w) * m_ratio, 0.f, m_imgWidth);
            float y0 = std::clamp((y - 0.5f * h) * m_ratio, 0.f, m_imgHeight);
            float x1 = std::clamp((x + 0.5f * w) * m_ratio, 0.f, m_imgWidth);
            float y1 = std::clamp((y + 0.5f * h) * m_ratio, 0.f, m_imgHeight);

            cv::Rect_<float> bbox;
            bbox.x = x0;
            bbox.y = y0;
            bbox.width = x1 - x0;
            bbox.height = y1 - y0;

            std::vector<float> kps;
            for (int k = 0; k < NUM_KPS; k++)
            {
                float kpsX = *(kps_ptr + 3 * k) * m_ratio;
                float kpsY = *(kps_ptr + 3 * k + 1) * m_ratio;
                float kpsS = *(kps_ptr + 3 * k + 2);
                kpsX = std::clamp(kpsX, 0.f, m_imgWidth);
                kpsY = std::clamp(kpsY, 0.f, m_imgHeight);
                kps.push_back(kpsX);
                kps.push_back(kpsY);
                kps.push_back(kpsS);
            }

            bboxes.push_back(bbox);
            labels.push_back(0); // All detected objects are people
            scores.push_back(score);
            kpss.push_back(kps);
        }
    }

    // Run NMS
    cv::dnn::NMSBoxesBatched(bboxes, scores, labels, PROBABILITY_THRESHOLD, NMS_THRESHOLD, indices);

    std::vector<Object> objects;

    // Choose the top k detections
    int cnt = 0;
    for (auto &chosenIdx : indices)
    {
        if (cnt >= TOP_K)
        {
            break;
        }

        Object obj{};
        obj.probability = scores[chosenIdx];
        obj.label = labels[chosenIdx];
        obj.rect = bboxes[chosenIdx];
        obj.kps = kpss[chosenIdx];
        objects.push_back(obj);

        cnt += 1;
    }

    return objects;
}

int YoloV8::postProcessClassify(std::vector<float> &featureVector)
{
    // 图像分类时
    int max_index;
    // 使用范围-based for 循环遍历并打印每个元素
    for (auto element : featureVector)
    {
        std::cout << element << " ";
    }
    std::cout << std::endl;
    // 使用 std::max_element 查找最大值
    auto max_it = std::max_element(featureVector.begin(), featureVector.end());
    int max_value;
    if (max_it != featureVector.end())
    {
        max_value = *max_it;
        // 计算最大值的索引
        max_index = std::distance(featureVector.begin(), max_it);
        // 输出图像分类时最大值和其索引值，再map到classes中找到对象。
        // std::cout << "Max element in vector: " << max_value << std::endl;
        std::cout << "Index of max element: " << max_index << std::endl;
    }
    else
    {
        std::cout << "Vector is empty" << std::endl;
    }
    // std::vector<Object> objects;
    // Object obj{};
    // obj.probability = max_value;
    // obj.label = max_index;

    // objects.push_back(obj);
    return max_index;
}

std::vector<Object> YoloV8::postProcessDetect(std::vector<float> &featureVector)
{
    const auto &outputDims = m_trtEngine->getOutputDims();
    auto numChannels = outputDims[0].d[1];
    auto numAnchors = outputDims[0].d[2];

    auto numClasses = CLASS_NAMES.size();
    std::vector<cv::Rect> bboxes;
    std::vector<float> scores;
    std::vector<int> labels;
    std::vector<int> indices;

    cv::Mat output = cv::Mat(numChannels, numAnchors, CV_32F, featureVector.data());

    output = output.t();

    // Get all the YOLO proposals
    for (int i = 0; i < numAnchors; i++)
    {
        auto rowPtr = output.row(i).ptr<float>();
        auto bboxesPtr = rowPtr;
        auto scoresPtr = rowPtr + 4;
        auto maxSPtr = std::max_element(scoresPtr, scoresPtr + numClasses);
        float score = *maxSPtr;
        if (score > PROBABILITY_THRESHOLD)
        {
            float x = *bboxesPtr++;
            float y = *bboxesPtr++;
            float w = *bboxesPtr++;
            float h = *bboxesPtr;

            float x0 = std::clamp((x - 0.5f * w) * m_ratio, 0.f, m_imgWidth);
            float y0 = std::clamp((y - 0.5f * h) * m_ratio, 0.f, m_imgHeight);
            float x1 = std::clamp((x + 0.5f * w) * m_ratio, 0.f, m_imgWidth);
            float y1 = std::clamp((y + 0.5f * h) * m_ratio, 0.f, m_imgHeight);

            int label = maxSPtr - scoresPtr;
            cv::Rect_<float> bbox;
            bbox.x = x0;
            bbox.y = y0;
            bbox.width = x1 - x0;
            bbox.height = y1 - y0;

            bboxes.push_back(bbox);
            labels.push_back(label);
            scores.push_back(score);
        }
    }

    // Run NMS
    cv::dnn::NMSBoxesBatched(bboxes, scores, labels, PROBABILITY_THRESHOLD, NMS_THRESHOLD, indices);

    std::vector<Object> objects;

    // Choose the top k detections
    int cnt = 0;
    for (auto &chosenIdx : indices)
    {
        if (cnt >= TOP_K)
        {
            break;
        }

        Object obj{};
        obj.probability = scores[chosenIdx];
        obj.label = labels[chosenIdx];
        obj.rect = bboxes[chosenIdx];
        objects.push_back(obj);

        cnt += 1;
    }

    return objects;
}

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
        cv::rectangle(image, detectionROI, cv::Scalar(0, 255, 0), 5);
    }

    // If segmentation information is present, start with that
    if (!objects.empty() && !objects[0].boxMask.empty())
    {
        std::cout << "have mask" << std::endl;
        for (const auto &object : objects)
        {
            // Choose the color
            int colorIndex = object.label % COLOR_LIST.size(); // We have only defined 80 unique colors
            cv::Scalar color = cv::Scalar(COLOR_LIST[colorIndex][0], COLOR_LIST[colorIndex][1], COLOR_LIST[colorIndex][2]);
            std::cout << "have mask processed" << std::endl;
        }
    }

    // Bounding boxes and annotations
    result = true; // 初始化结果
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
        int colorIndex = object.label % COLOR_LIST.size(); // We have only defined 80 unique colors
        cv::Scalar color = cv::Scalar(COLOR_LIST[colorIndex][0], COLOR_LIST[colorIndex][1], COLOR_LIST[colorIndex][2]);
        float meanColor = cv::mean(color)[0];
        cv::Scalar txtColor;
        if (meanColor > 0.5)
        {
            txtColor = cv::Scalar(0, 0, 0);
        }
        else
        {
            txtColor = cv::Scalar(255, 255, 255);
        }

        const auto &rect = object.rect;
        std::string label_name = CLASS_NAMES[object.label].c_str();
        std::cout << "label_name" << label_name << std::endl; //"chilun",   "keti" ,"luosi"
        std::cout << "label_id" << object.label << std::endl;
        std::cout << "label_rect" << object.rect << std::endl;
        std::cout << "probability  " << object.probability * 100 << std::endl;
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

bool YoloV8::getResult() // 是否检测到defects和折叠或者破边
{
    return result;
}

float YoloV8::calculateAveragePixelValue(const cv::Mat &image, const cv::Rect &rect)
{
    // 提取矩形区域内的像素值
    cv::Mat roi = image(rect);

    // 计算像素值的平均值
    cv::Scalar meanValue = cv::mean(roi);

    // 返回平均值
    return meanValue[0];
}

void YoloV8::setArea_threshold(float area)
{
    area_threshold = area;
}

void YoloV8::setIntensity_threshold(float intensity)
{
    intensity_threshold = intensity;
}

int YoloV8::getItemIndex()
{
    return Itemindex;
}

std::set<std::string> YoloV8::getSet()
{
    return stringSet;
}

// 设置检测区域
void YoloV8::setDetectionROI(const cv::Rect &roi)
{
    detectionROI = roi;
}

// 启用或禁用ROI检测
void YoloV8::enableROIDetection(bool enable)
{
    useROI = enable;
}
