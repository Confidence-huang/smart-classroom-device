# V3 新背景训练归档

权重、曲线及阶段报告见selected/；完整图片SHA、拆分和YOLO标签见dataset-manifest.json。原图不入库，需从本地采集档案按SHA恢复到images/（V1为stage2/images/）。

train.py和prepare*.py是当日工作脚本，依赖raw/、预标注和前序权重，不是下载即可运行的数据集。调用前需恢复数据，并把data.yaml的path设置为恢复目录绝对路径。args.yaml中的<local-data>只是脱敏路径。不能以模型预测替代缺失真值。

阶段报告保留原时点表述（可能仍为25人目标或未部署），最终目标0～20人及部署状态见../NEW-BACKGROUND-20261127.md。
