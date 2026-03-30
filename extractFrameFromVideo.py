import cv2
import os
import random
# Set the path to the video file
video_path = 'resource\\192.168.1.64_01_2026031014325577.mp4'
video = cv2.VideoCapture(video_path)
#产生一个随机整数
random_number = random.randint(50,60)
#从视频中随机抽取50张图像
a =1
while 1:
    ret, frame = video.read()
    cv2.imshow("as",frame)
    cv2.waitKey(1)
    a+=1
    if a %random_number==0:
        # Set the path to the directory where you want to save the images
        image_dir = 'resource\\images'
        # Create the directory if it doesn't exist
        if not os.path.exists(image_dir):
            os.makedirs(image_dir)
        # Save the image with a unique name
        image_path = os.path.join(image_dir, f'image_{a}.jpg')
        cv2.imwrite(image_path, frame)
        print("saved"+image_path)
    
# Release the video capture object
video.release()
# Close all OpenCV windows
cv2.destroyAllWindows()
    