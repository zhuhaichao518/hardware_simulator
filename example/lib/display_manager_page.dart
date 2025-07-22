import 'package:flutter/material.dart';
import 'package:hardware_simulator/hardware_simulator.dart';
import 'package:hardware_simulator/display_data.dart';

class DisplayManagerPage extends StatefulWidget {
  @override
  _DisplayManagerPageState createState() => _DisplayManagerPageState();
}

class _DisplayManagerPageState extends State<DisplayManagerPage> {
  List<DisplayData> displays = [];
  bool isLoading = false;
  
  String _selectedResolution = '1920x1080';
  int _selectedRefreshRate = 60;
  
  final List<String> _resolutions = [
    '1280x720',
    '1920x1080',
    '2560x1440', 
    '3840x2160'
  ];
  
  final List<int> _refreshRates = [24, 30, 60, 144, 240];

  @override
  void initState() {
    super.initState();
    _loadDisplays();
  }

  Future<void> _loadDisplays() async {
    setState(() {
      isLoading = true;
    });

    try {

        List<DisplayData> displayList = await HardwareSimulator.getDisplayList();
        displays = displayList;

    } catch (e) {
      print('Error loading displays: $e');
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('加载显示器列表失败: $e')),
      );
    }

    setState(() {
      isLoading = false;
    });
  }

  Future<void> _createDefaultDisplay() async {
    try {
      bool initialized = await HardwareSimulator.initParsecVdd();
      if (!initialized) {
        throw Exception('初始化parsec-vdd失败');
      }

      int displayId = await HardwareSimulator.createDisplay();
      print('创建默认显示器成功，ID: $displayId');
      
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('创建默认显示器成功，ID: $displayId')),
      );
      
      // 重新加载显示器列表
      await _loadDisplays();
    } catch (e) {
      print('Error creating default display: $e');
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('创建默认显示器失败: $e')),
      );
    }
  }

  Future<void> _createCustomDisplay() async {
    try {
      bool initialized = await HardwareSimulator.initParsecVdd();
      if (!initialized) {
        throw Exception('初始化parsec-vdd失败');
      }

      List<String> resolution = _selectedResolution.split('x');
      int width = int.parse(resolution[0]);
      int height = int.parse(resolution[1]);
      
      int displayId = await HardwareSimulator.createDisplayWithConfig(
        width, 
        height, 
        _selectedRefreshRate
      );
      
      print('创建自定义显示器成功，ID: $displayId，分辨率: ${width}x$height@${_selectedRefreshRate}Hz');
      
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('创建显示器成功，ID: $displayId')),
      );
      
      // 重新加载显示器列表
      await _loadDisplays();
    } catch (e) {
      print('Error creating custom display: $e');
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('创建自定义显示器失败: $e')),
      );
    }
  }

  Future<void> _removeDisplay(int displayId) async {
    try {
      bool removed = await HardwareSimulator.removeDisplay(displayId);
      if (removed) {
        print('删除显示器成功，ID: $displayId');
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('删除显示器成功，ID: $displayId')),
        );
        
        // 重新加载显示器列表
        await _loadDisplays();
      } else {
        throw Exception('删除失败');
      }
    } catch (e) {
      print('Error removing display: $e');
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('删除显示器失败: $e')),
      );
    }
  }

  void _showChangeDisplayDialog(DisplayData display) {
    String newResolution = '${display.width}x${display.height}';
    int newRefreshRate = display.refreshRate;

    showDialog(
      context: context,
      builder: (context) => StatefulBuilder(
        builder: (context, setDialogState) => AlertDialog(
          title: Text('修改显示器 ${display.index}'),
          content: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Text('当前配置: ${display.width}x${display.height}@${display.refreshRate}Hz'),
              SizedBox(height: 16),
              Row(
                children: [
                  Text('分辨率: '),
                  Expanded(
                    child: DropdownButton<String>(
                      value: newResolution,
                      isExpanded: true,
                      onChanged: (String? value) {
                        setDialogState(() {
                          newResolution = value!;
                        });
                      },
                      items: _resolutions.map<DropdownMenuItem<String>>((String value) {
                        return DropdownMenuItem<String>(
                          value: value,
                          child: Text(value),
                        );
                      }).toList(),
                    ),
                  ),
                ],
              ),
              SizedBox(height: 8),
              Row(
                children: [
                  Text('刷新率: '),
                  Expanded(
                    child: DropdownButton<int>(
                      value: newRefreshRate,
                      isExpanded: true,
                      onChanged: (int? value) {
                        setDialogState(() {
                          newRefreshRate = value!;
                        });
                      },
                      items: _refreshRates.map<DropdownMenuItem<int>>((int value) {
                        return DropdownMenuItem<int>(
                          value: value,
                          child: Text('${value}Hz'),
                        );
                      }).toList(),
                    ),
                  ),
                ],
              ),
            ],
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(context).pop(),
              child: Text('取消'),
            ),
            ElevatedButton(
              onPressed: () {
                Navigator.of(context).pop();
                _changeDisplay(display, newResolution, newRefreshRate);
              },
              child: Text('确认修改'),
            ),
          ],
        ),
      ),
    );
  }

  Future<void> _changeDisplay(DisplayData display, String resolution, int refreshRate) async {
    try {
      List<String> resolutionParts = resolution.split('x');
      int width = int.parse(resolutionParts[0]);
      int height = int.parse(resolutionParts[1]);
      
      bool success = await HardwareSimulator.changeDisplaySettings(
        display.index,
        width,
        height,
        refreshRate,
      );
      
      if (success) {
        print('显示器设置修改成功: ${display.index} -> ${width}x${height}@${refreshRate}Hz');
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('显示器设置修改成功')),
        );
        
        // 重新加载显示器列表
        await _loadDisplays();
      } else {
        throw Exception('修改失败');
      }
    } catch (e) {
      print('Error changing display: $e');
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('修改显示器失败: $e')),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('显示器管理'),
        actions: [
          IconButton(
            icon: Icon(Icons.refresh),
            onPressed: _loadDisplays,
          ),
        ],
      ),
      body: Padding(
        padding: EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // 创建显示器的控制区域
            Card(
              child: Padding(
                padding: EdgeInsets.all(16.0),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      '创建新显示器',
                      style: Theme.of(context).textTheme.headlineSmall,
                    ),
                    SizedBox(height: 16),
                    Row(
                      children: [
                        ElevatedButton(
                          onPressed: _createDefaultDisplay,
                          child: Text('创建默认显示器'),
                        ),
                        SizedBox(width: 16),
                        ElevatedButton(
                          onPressed: _loadDisplays,
                          child: Text('获取显示器列表'),
                        ),
                      ],
                    ),
                    SizedBox(height: 16),
                    Text('自定义显示器配置:'),
                    SizedBox(height: 8),
                    Row(
                      children: [
                        Text('分辨率: '),
                        DropdownButton<String>(
                          value: _selectedResolution,
                          onChanged: (String? newValue) {
                            setState(() {
                              _selectedResolution = newValue!;
                            });
                          },
                          items: _resolutions.map<DropdownMenuItem<String>>((String value) {
                            return DropdownMenuItem<String>(
                              value: value,
                              child: Text(value),
                            );
                          }).toList(),
                        ),
                        SizedBox(width: 20),
                        Text('刷新率: '),
                        DropdownButton<int>(
                          value: _selectedRefreshRate,
                          onChanged: (int? newValue) {
                            setState(() {
                              _selectedRefreshRate = newValue!;
                            });
                          },
                          items: _refreshRates.map<DropdownMenuItem<int>>((int value) {
                            return DropdownMenuItem<int>(
                              value: value,
                              child: Text('${value}Hz'),
                            );
                          }).toList(),
                        ),
                      ],
                    ),
                    SizedBox(height: 8),
                    ElevatedButton(
                      onPressed: _createCustomDisplay,
                      child: Text('创建自定义显示器 (${_selectedResolution}@${_selectedRefreshRate}Hz)'),
                    ),
                  ],
                ),
              ),
            ),
            SizedBox(height: 16),
            
            // 显示器列表
            Text(
              '显示器列表 (${displays.length}个)',
              style: Theme.of(context).textTheme.headlineSmall,
            ),
            SizedBox(height: 8),
            
            if (isLoading)
              Center(child: CircularProgressIndicator())
            else if (displays.isEmpty)
              Card(
                child: Padding(
                  padding: EdgeInsets.all(32.0),
                  child: Center(
                    child: Column(
                      children: [
                        Icon(Icons.display_settings, size: 64, color: Colors.grey),
                        SizedBox(height: 16),
                        Text('暂无显示器', style: TextStyle(color: Colors.grey)),
                        SizedBox(height: 8),
                        Text('点击上方按钮创建或获取显示器', style: TextStyle(color: Colors.grey, fontSize: 12)),
                      ],
                    ),
                  ),
                ),
              )
            else
              Expanded(
                child: ListView.builder(
                  itemCount: displays.length,
                  itemBuilder: (context, index) {
                    DisplayData display = displays[index];
                    return Card(
                      margin: EdgeInsets.only(bottom: 8.0),
                      child: ListTile(
                        leading: CircleAvatar(
                          backgroundColor: display.isVirtual ? Colors.blue : Colors.green,
                          child: Text('${display.index}'),
                        ),
                        title: Text(display.displayName),
                        subtitle: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text('分辨率: ${display.width}x${display.height}'),
                            Text('刷新率: ${display.refreshRate}Hz'),
                            Text('类型: ${display.isVirtual ? "虚拟显示器" : "物理显示器"}'),
                          ],
                        ),
                        trailing: Row(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            if (display.isVirtual) ...[
                              IconButton(
                                icon: Icon(Icons.edit, color: Colors.blue),
                                onPressed: () => _showChangeDisplayDialog(display),
                                tooltip: '修改',
                              ),
                              IconButton(
                                icon: Icon(Icons.delete, color: Colors.red),
                                onPressed: () => _removeDisplay(display.displayUid),
                                tooltip: '删除',
                              ),
                            ] else
                              Icon(Icons.lock, color: Colors.grey, size: 20),
                          ],
                        ),
                        isThreeLine: true,
                      ),
                    );
                  },
                ),
              ),
          ],
        ),
      ),
    );
  }
}
