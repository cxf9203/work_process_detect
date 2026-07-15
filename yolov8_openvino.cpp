#define _CRT_SECURE_NO_WARNINGS
#include "yolov8_openvino.h"
#include <opencv2/cudaimgproc.hpp>

YoloV8::YoloV8(const std::string &modelPath, const YoloV8Config &config)
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
    // Create our TensorRT inference engine   openvino
    // Step 1. Initialize OpenVINO Runtime Core
    ov::Core core;

    // Step 2. Read the model
    std::shared_ptr<ov::Model> model = core.read_model(modelPath);

    // Preprocessing setup for the model
    ov::preprocess::PrePostProcessor ppp = ov::preprocess::PrePostProcessor(model);

    ppp.input().tensor().set_element_type(ov::element::u8).set_layout("NHWC").set_color_format(ov::preprocess::ColorFormat::BGR);
    ppp.input().preprocess().convert_element_type(ov::element::f32).convert_color(ov::preprocess::ColorFormat::RGB).scale({255, 255, 255});
    ppp.input().model().set_layout("NCHW");
    // 设置第一个输出的元素类型
    ppp.output(0).tensor().set_element_type(ov::element::f32);
    // 设置第二个输出的元素类型
    if (model->outputs().size() > 1)
    {
        // 设置第二个输出的元素类型
        ppp.output(1).tensor().set_element_type(ov::element::f32);
    }
    else
    {
        std::cout << "The model does not have a second output. Skipping setting its element type." << std::endl;
    }

    model = ppp.build(); // Build the preprocessed model

    // Compile the model for inference
    compiled_model = core.compile_model(model, "GPU"); // OPENVINO intel集成显卡推理
    infer_request = compiled_model.create_infer_request(); // Create inference request

    // Get input shape from the model
    const std::vector<ov::Output<ov::Node>> inputs = model->inputs();
    const ov::Shape input_shape = inputs[0].get_shape();
    height = input_shape[1];
    width = input_shape[2];
    std::cout << "input_shape is:" << input_shape[0] << " " << input_shape[1] << " " << input_shape[2] << " " << input_shape[3] << " " << std::endl;

    // Get output shape from the model
    const std::vector<ov::Output<ov::Node>> outputs = model->outputs(); // 获取输出维度
    const ov::Shape output_shape0 = outputs[0].get_shape();
    numOutputs = outputs.size();    // 输出维度
    numChannels = output_shape0[1]; // 输出通道数
    std::cout << "output_shape0 is:" << output_shape0[0] << " " << output_shape0[1] << " " << output_shape0[2] << std::endl;
    std::cout << outputs.size() << "outputs" << numChannels << "numChannels" << std::endl;
    if (numOutputs > 1)
    { // seg
        const ov::Shape output_shape1 = outputs[1].get_shape();
        std::cout << "output_shape1 is:" << output_shape1[0] << " " << output_shape1[1] << " " << output_shape1[2] << output_shape1[3] << std::endl;
    }
}

std::vector<Object> YoloV8::detectObjects(cv::Mat inputImageBGR)
{
    std::vector<std::vector<std::vector<float>>> featureVectors;
    // Preprocess the input image
    cv::Mat input = preprocess(inputImageBGR);
    // Run inference using the TensorRT engine
    std::vector<Object> ret;
    std::vector<std::vector<float>> featureVector;
    // 根据输出情况，实现目标检测、语义分割、分类、pose，以下为seg
    if (numOutputs == 1)
    {
        std::cout << "Object detection or pose estimation" << std::endl;
        // Object detection or pose estimation
        // Since we have a batch size of 1 and only 1 output, we must convert the output from a 3D array to a 1D array.
        /* std::vector<float> featureVector;
        Engine<float>::transformOutput(featureVectors, featureVector); */

        // TODO: Need to improve this to make it more generic (don't use magic number).
        // For now it works with Ultralytics pretrained models.
        if (numChannels == 56)
        {
            std::cout << "Pose estimation" << std::endl;
            // Pose estimation
            ret = postProcessPose(inputImageBGR);
        }
        else
        {
            std::cout << "Object detection or classify" << std::endl;
            // Object detection or classify
            ret = postProcessDetect(inputImageBGR);
            // int classIdx = postProcessClassify(inputImageBGR);
            // std::cout << "classIdx" << classIdx << std::endl;
        }
    }
    else
    {
        std::cout << "Segmentation" << std::endl;
        // Segmentation
        // Since we have a batch size of 1 and 2 outputs, we must convert the output from a 3D array to a 2D array.
        ret = postProcessSegmentation(inputImageBGR);
    }
    return ret;
}

cv::Mat YoloV8::preprocess(const cv::Mat &gpuImg)
{
    // Convert the image from BGR to RGB
    cv::Mat rgbMat = gpuImg.clone();
    // cv::cuda::cvtColor(gpuImg, rgbMat, cv::COLOR_BGR2RGB);

    auto resized = rgbMat;
    std::cout << resized.rows << "resized.rows " << resized.cols << "resized.cols" << "/n" << height << "height" << width << "width" << std::endl;
    // Resize to the model expected input size while maintaining the aspect ratio with the use of padding
    if (resized.rows != height || resized.cols != width)
    {
        // Only resize if not already the right size to avoid unecessary copy
        resized = resizeKeepAspectRatioPadRightBottom(rgbMat, height, width, cv::Scalar(0, 0, 0));
    }
    // cv::imshow("resized", resized);
    // cv::waitKey(0);

    // Convert to format expected by our inference engine
    // The reason for the strange format is because it supports models with multiple inputs as well as batching
    // In our case though, the model only has a single input and we are using a batch size of 1.
    /* std::vector<cv::cuda::GpuMat> input{ std::move(resized) };
     std::vector<std::vector<cv::cuda::GpuMat>> inputs{ std::move(input) };*/

    // These params will be used in the post-processing stage
    m_imgHeight = rgbMat.rows;
    m_imgWidth = rgbMat.cols;
    m_ratio = 1.f / std::min(width / static_cast<float>(rgbMat.cols), height / static_cast<float>(rgbMat.rows));
    float *input_data = (float *)resized.data; // Get pointer to resized frame data
    const ov::Tensor input_tensor = ov::Tensor(compiled_model.input().get_element_type(), compiled_model.input().get_shape(), input_data); // Create input tensor
    infer_request.set_input_tensor(input_tensor); // Set input tensor for inference
    infer_request.infer(); // Run inference
    std::cout << " Run inference";
    return resized;
}

cv::Mat YoloV8::resizeKeepAspectRatioPadRightBottom(const cv::Mat &input, size_t height, size_t width, const cv::Scalar &bgcolor)
{
    float r = std::min(width / (input.cols * 1.0), height / (input.rows * 1.0));
    int unpad_w = r * input.cols;
    int unpad_h = r * input.rows;
    cv::Mat re(unpad_h, unpad_w, CV_8UC3);
    cv::resize(input, re, re.size());
    cv::Mat out(height, width, CV_8UC3, bgcolor);
    re.copyTo(out(cv::Rect(0, 0, re.cols, re.rows)));
    return out;
}

std::vector<Object> YoloV8::postProcessDetect(cv::Mat gpuImg)
{
    cv::Mat input_image = gpuImg.clone();
    // Get the output tensors
    auto output_tensor_0 = infer_request.get_output_tensor(0); // detect_data
    float *output_data_0 = output_tensor_0.data<float>();      // 提取目标检测相关信息
    // 获取第一个输出张量的形状
    ov::Shape shape_0 = output_tensor_0.get_shape();
    std::cout << "Output 0 shape: ";
    for (size_t i = 0; i < shape_0.size(); ++i)
    {
        std::cout << shape_0[i];
        if (i < shape_0.size() - 1)
        {
            std::cout << " x ";
        }
    }
    std::cout << std::endl;
    numChannels = shape_0[1];
    numAnchors = shape_0[2];
    int numClasses = CLASS_NAMES.size();
    std::vector<cv::Rect> bboxes;
    std::vector<float> scores;
    std::vector<int> labels;
    std::vector<int> indices;
    cv::Mat output = cv::Mat(numChannels, numAnchors, CV_32F, output_data_0);
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

    // Choose the top k detections
    std::vector<Object> objs;
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
        objs.push_back(obj);
        cnt += 1;
    }

    return objs;
}

std::vector<Object> YoloV8::postProcessSegmentation(cv::Mat gpuImg)
{
    cv::Mat input_image = gpuImg.clone();
    // Get the output tensors
    auto output_tensor_0 = infer_request.get_output_tensor(0); // detect_data
    float *output_data_0 = output_tensor_0.data<float>();      // 提取目标检测相关信息
    // 获取第一个输出张量的形状
    ov::Shape shape_0 = output_tensor_0.get_shape();
    std::cout << "Output 0 shape: ";
    for (size_t i = 0; i < shape_0.size(); ++i)
    {
        std::cout << shape_0[i];
        if (i < shape_0.size() - 1)
        {
            std::cout << " x ";
        }
    }
    std::cout << std::endl;

    auto output_tensor_1 = infer_request.get_output_tensor(1); // proto_data
    float *output_data_1 = output_tensor_1.data<float>();
    // 获取第二个输出张量的形状
    ov::Shape shape_1 = output_tensor_1.get_shape();
    std::cout << "Output 1 shape: ";
    for (size_t i = 0; i < shape_1.size(); ++i)
    {
        std::cout << shape_1[i];
        if (i < shape_1.size() - 1)
        {
            std::cout << " x ";
        }
    }
    std::cout << std::endl;
    numChannels = shape_0[1];
    numAnchors = shape_0[2];
    int numClasses = numChannels - SEG_CHANNELS - 4; // 80默认
    std::cout << numChannels << "numChannels" << numAnchors << "numAnchors" << numClasses << "numClasses" << std::endl;
    // 解析检测输出形状 [1, 116, 8400]
    // auto detect_shape = output0.get_shape();
    // const int numChannels = detect_shape[1];
    // const int numAnchors = detect_shape[2];

    // 解析原型掩码形状 [1, 32, 160, 160]
    // auto mask_shape = output1.get_shape();
    // const int channels = mask_shape[1];
    // const int mask_w = mask_shape[2];
    // const int mask_h = mask_shape[3];

    std::vector<int> labels;
    std::vector<float> scores;
    std::vector<cv::Rect> bboxes;
    std::vector<cv::Mat> maskConfs;
    std::vector<int> indices;
    cv::Mat output = cv::Mat(numChannels, numAnchors, CV_32F, output_data_0);
    output = output.t();

    cv::Mat protos = cv::Mat(SEG_CHANNELS, SEG_H * SEG_W, CV_32F, output_data_1);
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

    // Run NMS
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

    // Convert segmentation mask to original frame
    if (!masks.empty())
    {
        cv::Mat matmulRes = (masks * protos).t();
        cv::Mat maskMat = matmulRes.reshape(indices.size(), {SEG_W, SEG_H});

        std::vector<cv::Mat> maskChannels;
        cv::split(maskMat, maskChannels);

        cv::Rect roi;
        if (m_imgHeight > m_imgWidth)
        {
            roi = cv::Rect(0, 0, SEG_W * m_imgWidth / m_imgHeight, SEG_H);
        }
        else
        {
            roi = cv::Rect(0, 0, SEG_W, SEG_H * m_imgHeight / m_imgWidth);
        }

        for (size_t i = 0; i < indices.size(); i++)
        {
            cv::Mat dest, mask;
            cv::exp(-maskChannels[i], dest);
            dest = 1.0 / (1.0 + dest);
            dest = dest(roi);
            cv::resize(dest, mask, cv::Size(static_cast<int>(m_imgWidth), static_cast<int>(m_imgHeight)), cv::INTER_LINEAR);
            objs[i].boxMask = mask(objs[i].rect) > SEGMENTATION_THRESHOLD;
        }
    }

    return objs;
}

int YoloV8::postProcessClassify(cv::Mat gpuImg)
{
    // Get the output tensors
    auto output_tensor_0 = infer_request.get_output_tensor(0); // detect_data
    float *output_data_0 = output_tensor_0.data<float>();      // 提取图像分类相关信息
    // 获取第一个输出张量的形状
    ov::Shape shape_0 = output_tensor_0.get_shape();
    std::cout << "Output 0 shape: ";
    for (size_t i = 0; i < shape_0.size(); ++i)
    {
        std::cout << shape_0[i];
        if (i < shape_0.size() - 1)
        {
            std::cout << " x ";
        }
    }
    std::cout << shape_0.size() << "shape_0.size()" << std::endl;
    if (shape_0.size() != 2)
    {
        // 只有图像分类时为2
        return 0;
    }
    // 假设输出维度为 1 x 1000，找到分类结果
    int num_classes = shape_0[1]; // 类别数量，这里是 1000
    float max_score = output_data_0[0];
    int max_index = 0;
    std::cout << output_data_0[0] << "output_data_0" << std::endl;
    // 遍历所有类别得分，找到得分最高的类别
    for (int i = 1; i < num_classes; ++i)
    {
        if (output_data_0[i] > max_score)
        {
            max_score = output_data_0[i];
            max_index = i;
        }
    }
    std::vector<Object> objects;
    Object obj{};
    obj.probability = max_score;
    obj.label = max_index;
    objects.push_back(obj);
    return max_index;
}

std::vector<Object> YoloV8::postProcessPose(cv::Mat gpuImg)
{
    cv::Mat input_image = gpuImg.clone();
    // Get the output tensors
    auto output_tensor_0 = infer_request.get_output_tensor(0); // detect_data
    float *output_data_0 = output_tensor_0.data<float>();      // 提取目标检测相关信息
    // 获取第一个输出张量的形状
    ov::Shape shape_0 = output_tensor_0.get_shape();
    std::cout << "Output 0 shape: ";
    for (size_t i = 0; i < shape_0.size(); ++i)
    {
        std::cout << shape_0[i];
        if (i < shape_0.size() - 1)
        {
            std::cout << " x ";
        }
    }
    std::cout << std::endl;
    numChannels = shape_0[1];
    numAnchors = shape_0[2];

    std::vector<cv::Rect> bboxes;
    std::vector<float> scores;
    std::vector<int> labels;
    std::vector<int> indices;
    std::vector<std::vector<float>> kpss;

    cv::Mat output = cv::Mat(numChannels, numAnchors, CV_32F, output_data_0);
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

    // Choose the top k detections
    std::vector<Object> objs;
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
        objs.push_back(obj);
        cnt += 1;
    }

    return objs;
}
