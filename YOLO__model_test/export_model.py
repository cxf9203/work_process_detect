from ultralytics   import YOLO

model = YOLO("YOLO__model_test/process.pt")
model.export(format="onnx") # or "engine", "tflite", "coreml", "saved_model", "tf", "torchscript", "jit"