from ultralytics import YOLO
import cv2
import numpy as np

# Load the YOLO model
model = YOLO("best.pt")
labelSet = ["chilun", "keti", "luosi"]
# Open the video file
#video = cv2.VideoCapture("weights/192.168.1.64_01_20260317141519973_1.mp4")
video = cv2.VideoCapture("rtsp://admin:CXF643200@192.168.1.64/Streaming/Channels/1")  # 海康相机ip
# Get the video properties
width = int(video.get(cv2.CAP_PROP_FRAME_WIDTH))
height = int(video.get(cv2.CAP_PROP_FRAME_HEIGHT))
fps = video.get(cv2.CAP_PROP_FPS)
CHILUN_NUM = 1#标准齿轮数
LUOSI_NUM = 4#标准螺丝数
res_flag = 0
chilun_flag = 0
luosi_flag = 0
cur_keti = 0
last_keti = 0
#yolo model predict
while True:
    # Read a frame from the video
    ret, frame_ori = video.read()
    frame = cv2.resize(frame_ori, (720, 640))
    if not ret:
        break

    # Perform object detection on the frame
    results = model(frame)
    keti = 0
    chilun = 0
    luosi = 0
    # Draw the bounding boxes and labels on the frame
    for result in results:
        boxes = result.boxes
        for box in boxes: 
            x1, y1, x2, y2 = box.xyxy[0]
            x1, y1, x2, y2 = int(x1), int(y1), int(x2), int(y2)
            labelidx = box.cls[0].item()
            label = labelSet[int(labelidx)]
            confidence = box.conf[0].item()
            cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 255, 0), 2)
            cv2.putText(frame, f"{label} {confidence:.2f}", (x1, y1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.9, (0, 255, 0), 2)
            if label == "keti":
                keti += 1
            elif label == "chilun":
                chilun += 1
            elif label == "luosi":
                luosi += 1
    print("keti:", keti, "chilun:", chilun, "luosi:", luosi)
    # 将统计数量显示在图像左上角
    cv2.putText(frame, f"keti: {keti}", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
    cv2.putText(frame, f"chilun: {chilun}", (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
    cv2.putText(frame, f"luosi: {luosi}", (10, 90), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
    #视频流检测目标是否数量对齐，keti出现时开始计数，keti消失时停止计数 ，若chilun和luosi数量与标准值对齐则显示对齐，否则显示不对齐
    cur_keti = keti
    if keti > 0:
        if chilun == CHILUN_NUM :
            #满了 plc res_flag置1
            #cv2.putText(frame, "OK", (10, 120), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
            chilun_flag = 1
        if luosi == LUOSI_NUM:
            #满了 plc res_flag置1
            #cv2.putText(frame, "OK", (10, 130), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
            luosi_flag = 1
    
    last_keti = cur_keti
    if chilun_flag and not luosi_flag:
        #绘制消息框
        cv2.putText(frame, "chilun OK", (10, 190), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
        cv2.putText(frame, "luosi not yet", (10, 210), cv2.FONT_HERSHEY_SIMPLEX, 1,(0, 255, 0), 2)
    if luosi_flag and not chilun_flag:
        #绘制消息框
        cv2.putText(frame, "chilun OK", (10, 190), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
        cv2.putText(frame, "luosi OK", (10, 210), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
    if chilun_flag and luosi_flag:
        #绘制消息框
        cv2.putText(frame, "chilun OK", (10, 190), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
        cv2.putText(frame, "luosi OK", (10, 210), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
        cv2.putText(frame, "ALL OK", (10, 290), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
    if cur_keti==0 and last_keti==1:
        #keti消失，chilun_flag和luosi_flag置0
        chilun_flag = 0
        luosi_flag = 0
        #并检查是否漏装对齐
        if not chilun_flag:
            #绘制消息框
            cv2.putText(frame, "chilun miss", (10, 210), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
        if not luosi_flag:
            #绘制消息框 
            cv2.putText(frame, "luosi miss", (10, 230), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
    if cur_keti==0 and last_keti==0:
        #keti消失，chilun_flag和luosi_flag置0
        chilun_flag = 0
        luosi_flag = 0
        
    # Display the frame 
    cv2.imshow("YOLOv8 Object Detection", frame)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

# Release the video file and close the window
video.release()