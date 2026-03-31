from ultralytics   import YOLO
import cv2
model = YOLO("YOLO__model_test/best.pt")
#查看model的label信息
print(model.names)
# {0: 'chilun', 1: 'keti', 2: 'luosi', 3: 'luosi_left_bottom', 4: 'luosi_left_top', 5: 'luosi_right_bottom', 6: 'luosi_right_top', 7: 'place_chilun'}
labelSet = ["chilun", "keti", "luosi", "luosi_left_bottom", "luosi_left_top", "luosi_right_bottom", "luosi_right_top", "place_chilun"]
video_path = "video/work_process.mp4"
# image_path = "saved_img/frame_0007.jpg"
model.predict(source=video_path, save=True, show=False)
# results = model.predict(source=image_path, save=True, show=True)
# img = cv2.imread(image_path)
# print(results)#打印结果
# #使用官方yolo方式将标签信息显示在图片上
# cv2.imshow("result", results[0].plot())
# cv2.waitKey(0)


# #有哪些检测到的label请打印出来
# # Draw the bounding boxes and labels on the frame
# for result in results:
#     boxes = result.boxes
#     for box in boxes:
#         x1, y1, x2, y2 = box.xyxy[0]
#         x1, y1, x2, y2 = int(x1), int(y1), int(x2), int(y2)
#         labelidx = box.cls[0].item()
#         label = labelSet[int(labelidx)]
#         confidence = box.conf[0].item()
#         cv2.rectangle(img, (x1, y1), (x2, y2), (0, 255, 0), 2)
#         cv2.putText(img, f"{label} {confidence:.2f}", (x1, y1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.9, (0, 255, 0), 2)
# # Display the frame
# cv2.imshow("YOLOv8 Object Detection", img)
# cv2.waitKey(0)
# cv2.destroyAllWindows()


