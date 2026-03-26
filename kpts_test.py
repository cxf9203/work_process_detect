from ultralytics import YOLO
import cv2
# Load a model
model = YOLO("key_points.pt")  # pretrained YOLO26n model
video = cv2.VideoCapture("resource\\192.168.1.64_01_2026031014325577.mp4")
while 1:
    ret , frame = video.read()
    # Run batched inference on a list of images
    results = model(frame)  # return a list of Results objects

    # Process results list
    for result in results:
        boxes = result.boxes  # Boxes object for bounding box outputs
        masks = result.masks  # Masks object for segmentation masks outputs
        keypoints = result.keypoints  # Keypoints object for pose outputs
        probs = result.probs  # Probs object for classification outputs
        obb = result.obb  # Oriented boxes object for OBB outputs
        # result.show()  # display to screen
        # result.save(filename="result.jpg")  # save to disk
    #显示每一张图片的预测结果并将结果绘制在frame中
    frame = result.plot()
    cv2.imshow("frame",frame)
    cv2.waitKey(1)
video.release()
cv2.destroyAllWindows()