# EcoVision 综合舒适度算法的研究依据

对应 [算法设计 v0.1](COMFORT-DESIGN.md)。外部研究支持测量维度、建模思路和部分设备联动。四项具体权重、最差项加 20 分的上限、人数积分、触发阈值与防抖时长由项目提出，均待校准。

| 算法设计 | 原始研究或技术依据 | 可采用的结论与适用边界 |
|---|---|---|
| 分项评价和总体评价同时保留 | Frontczak 等（2012），*Indoor Air*, 22, 119–131。研究分析了 351 栋办公楼的 52,980 份使用者调查。[原文](https://doi.org/10.1111/j.1600-0668.2011.00745.x) | 研究为同步采集分项与总体满意度提供方法参考。办公楼数据不能直接推出高校教室的权重。研究也涵盖空间和声环境，EcoVision 原型当前只有四项 |
| 温度和通风分开判断 | Wargocki 与 Wyon（2007），*HVAC&R Research*, 13, 193–220，开展两项学校教室现场干预，分别改变室温和室外送风。[原文](https://doi.org/10.1080/10789669.2007.10390951) | 支持把温度和通风当作可独立干预的变量。研究包含儿童课堂任务表现，不能将结论直接等同于高校学生舒适度或本项目阈值 |
| 温湿度作为热环境代理 | ANSI/ASHRAE Standard 55-2023。模型同时考虑温度、热辐射、湿度、风速、活动和着衣条件。[官方范围](https://www.ashrae.org/technical-resources/bookstore/standard-55-thermal-environmental-conditions-for-human-occupancy) | AHT20 提供其中两项。23–27℃、40–60%RH 和两条阶梯评分曲线是项目初值，评分属于环境指标 |
| CO₂触发通风评估 | ASHRAE（2025），*Position Document on Indoor Carbon Dioxide*。[立场文件](https://www.ashrae.org/file%20library/about/position%20documents/pd-on-indoor-carbon-dioxide-english.pdf) | CO₂ 结合室外浓度、占用及混合等条件可辅助判断通风。单一数值不能表征所有场地的换气率或整体 IAQ。800／1500 ppm 评分点和 1000 ppm 持续触发是工程假设，不能当成合规或健康界限 |
| 灯光与窗帘协调 | Lee、DiBartolomeo 与 Selkowitz（1998），*Energy and Buildings*, 29(1)，全尺寸办公室同步控制百叶与可调光照明，目标包含遮挡直射和维持工作面照度。[论文与摘要](https://smartersmallbuildings.lbl.gov/publications/thermal-and-daylighting-performance) | 支持以工作面照度为反馈协调自然光和补光。论文没有验证 EcoVision 的照度档位、等待时间和节能结果。GB 7793-2025 提供中小学校教室照明测量与评价参考，其适用范围和多指标要求应单独核对。[标准全文](https://www.ndcpa.gov.cn/jbkzzx/c100202/2019767782478352384/DfuK7Vv3.pdf) |
| 风扇与制冷协调 | Schiavon 等（2017），*Indoor Air*, 27, 690–702，56 名适应热带气候的受试者参加气候室实验，比较 23、26、29℃ 及个体可控风扇条件。[原文](https://doi.org/10.1111/ina.12352) | 在该实验条件下，可控气流能改善较暖环境的热舒适。桌面风扇的效果取决于人所在位置和实际风速，29℃也不是本项目的通用空调启动阈值 |
| 人数用于设备调度 | Erickson、Carreira-Perpiñán 与 Cerpa（2011），*OBSERVE: Occupancy-Based System for Efficient Reduction of HVAC Energy*，IPSN 2011。[作者报告](https://faculty.ucmerced.edu/mcarreira-perpinan-converted/papers/ipsn11-slides.pdf) | 说明人数信息可服务 HVAC 调度。EcoVision 的 3 帧／30 秒来自现有契约，人数乘分项缺口的积分属于项目提出的运行指标，不能解释成实际不适人数 |

## 如何验证自定义部分

1. 实测对齐。保存有效的工作面照度、CO₂、温湿度、人数、执行器反馈和匿名评价。按不同日期或课堂分成校准和留出验证数据，避免同一节课同时用于拟合和验证。
2. 评分消融。用同一批记录比较等权平均、35/15/30/20 加权、加权后加最低分上限。分别检查与总体满意度的秩相关、单项异常被高分掩盖的比例，以及温度或通风过差时误判为高分的比例。单项异常由预先定义的独立参考条件判定，避免用同一评分公式给自己当标签。
3. 联动对照。相同输入下比较原灯扇规则、独立阈值控制和带联锁控制。记录不允许的开窗请求、相互冲突的命令、错误关灯、重复动作和等待时间。真实环境是否改善以实物反馈和现场复测为准。
4. 同时记录反馈快慢。温湿度、照度、CO₂和机械位置的响应时间不同。最差项上限用于显示和排序，自动动作仍由有效测量、持续时间、人工模式与设备许可确定。
