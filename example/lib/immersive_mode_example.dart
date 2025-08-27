import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:hardware_simulator/hardware_simulator.dart';

class ImmersiveModeExample extends StatefulWidget {
  const ImmersiveModeExample({super.key});

  @override
  State<ImmersiveModeExample> createState() => _ImmersiveModeExampleState();
}

class _ImmersiveModeExampleState extends State<ImmersiveModeExample> {
  bool _immersiveModeEnabled = false;
  final List<String> _blockedKeys = [];
  final List<String> _pressedKeys = [];
  final FocusNode _focusNode = FocusNode();
  final FocusScopeNode _fsnode = FocusScopeNode();

  @override
  void initState() {
    super.initState();
    // 注册按键被拦截的回调
    HardwareSimulator.addKeyBlocked(_onKeyBlocked);
    // 请求焦点
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _focusNode.requestFocus();
    });
  }

  @override
  void dispose() {
    // 移除回调
    HardwareSimulator.removeKeyBlocked(_onKeyBlocked);
    _focusNode.dispose();
    _fsnode.dispose();
    super.dispose();
  }

  void _onKeyBlocked(int keyCode, bool isDown) {
    setState(() {
      String keyName = _getKeyName(keyCode);
      String action = isDown ? "按下" : "释放";
      _blockedKeys.add('$keyName $action (${DateTime.now().toString()})');
      // 保持最新的10条记录
      if (_blockedKeys.length > 10) {
        _blockedKeys.removeAt(0);
      }
    });
  }

  String _getKeyName(int keyCode) {
    switch (keyCode) {
      case 0x1B: // VK_ESCAPE
        return 'Escape';
      case 0x09: // VK_TAB
        return 'Tab';
      case 0x73: // VK_F4
        return 'F4';
      case 0x5B: // VK_LWIN
      case 0x5C: // VK_RWIN
        return 'Windows Key';
      default:
        return 'Key $keyCode';
    }
  }

  String _getKeyNameFromEvent(KeyEvent event) {
    if (event is KeyDownEvent || event is KeyUpEvent) {
      final keyCode = event.logicalKey.keyLabel;
      final isDown = event is KeyDownEvent;
      return '$keyCode (${isDown ? "按下" : "释放"})';
    }
    return 'Unknown';
  }

  void _onKeyPressed(KeyEvent event) {
    if (event is KeyDownEvent || event is KeyUpEvent) {
      setState(() {
        final keyInfo = _getKeyNameFromEvent(event);
        _pressedKeys.add('$keyInfo (${DateTime.now().toString()})');
        // 保持最新的20条记录
        if (_pressedKeys.length > 20) {
          _pressedKeys.removeAt(0);
        }
      });
    }
  }

  Future<void> _toggleImmersiveMode() async {
    try {
      final success = await HardwareSimulator.putImmersiveModeEnabled(!_immersiveModeEnabled);
      if (success) {
        setState(() {
          _immersiveModeEnabled = !_immersiveModeEnabled;
        });
        
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(_immersiveModeEnabled 
              ? '沉浸模式已启用 - 快捷键将被拦截' 
              : '沉浸模式已禁用 - 快捷键恢复正常'),
            backgroundColor: _immersiveModeEnabled ? Colors.green : Colors.orange,
          ),
        );
      } else {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('操作失败，请检查权限'),
            backgroundColor: Colors.red,
          ),
        );
      }
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('错误: $e'),
          backgroundColor: Colors.red,
        ),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return FocusScope(
      node: _fsnode,
      onKey: (data, event) {
        return KeyEventResult.handled;
      },
      child: KeyboardListener(
        focusNode: _focusNode,
        onKeyEvent: (event) {
          if (event is KeyDownEvent || event is KeyUpEvent) {
            _onKeyPressed(event);
          }
        },
        child: Scaffold(
          appBar: AppBar(
            title: const Text('沉浸模式示例'),
            backgroundColor: _immersiveModeEnabled ? Colors.green : Colors.blue,
          ),
          body: Padding(
            padding: const EdgeInsets.all(16.0),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Card(
                  child: Padding(
                    padding: const EdgeInsets.all(16.0),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          '沉浸模式状态: ${_immersiveModeEnabled ? "已启用" : "已禁用"}',
                          style: Theme.of(context).textTheme.titleLarge?.copyWith(
                            color: _immersiveModeEnabled ? Colors.green : Colors.orange,
                          ),
                        ),
                        const SizedBox(height: 8),
                        Text(
                          '沉浸模式启用后，以下快捷键将被拦截：',
                          style: Theme.of(context).textTheme.bodyMedium,
                        ),
                        const SizedBox(height: 8),
                        const Text('• Alt+Tab (切换窗口)'),
                        const Text('• Alt+F4 (关闭窗口)'),
                        const Text('• Windows键'),
                        const Text('• Ctrl+Alt+Esc (任务管理器)'),
                        const SizedBox(height: 16),
                        ElevatedButton(
                          onPressed: _toggleImmersiveMode,
                          style: ElevatedButton.styleFrom(
                            backgroundColor: _immersiveModeEnabled ? Colors.red : Colors.green,
                            foregroundColor: Colors.white,
                          ),
                          child: Text(_immersiveModeEnabled ? '禁用沉浸模式' : '启用沉浸模式'),
                        ),
                      ],
                    ),
                  ),
                ),
                const SizedBox(height: 16),
                Row(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Expanded(
                      child: Card(
                        child: Padding(
                          padding: const EdgeInsets.all(16.0),
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              Text(
                                '被拦截的按键记录',
                                style: Theme.of(context).textTheme.titleMedium,
                              ),
                              const SizedBox(height: 8),
                              if (_blockedKeys.isEmpty)
                                const Text(
                                  '暂无被拦截的按键',
                                  style: TextStyle(fontStyle: FontStyle.italic, color: Colors.grey),
                                )
                              else
                                ...(_blockedKeys.reversed.map((key) => Padding(
                                  padding: const EdgeInsets.symmetric(vertical: 2.0),
                                  child: Text(
                                    '🔒 $key',
                                    style: const TextStyle(fontFamily: 'monospace'),
                                  ),
                                ))),
                              if (_blockedKeys.isNotEmpty) ...[
                                const SizedBox(height: 8),
                                TextButton(
                                  onPressed: () {
                                    setState(() {
                                      _blockedKeys.clear();
                                    });
                                  },
                                  child: const Text('清空记录'),
                                ),
                              ],
                            ],
                          ),
                        ),
                      ),
                    ),
                    const SizedBox(width: 16),
                    Expanded(
                      child: Card(
                        child: Padding(
                          padding: const EdgeInsets.all(16.0),
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              Text(
                                '按键事件记录',
                                style: Theme.of(context).textTheme.titleMedium,
                              ),
                              const SizedBox(height: 8),
                              if (_pressedKeys.isEmpty)
                                const Text(
                                  '暂无按键事件',
                                  style: TextStyle(fontStyle: FontStyle.italic, color: Colors.grey),
                                )
                              else
                                ...(_pressedKeys.reversed.map((key) => Padding(
                                  padding: const EdgeInsets.symmetric(vertical: 2.0),
                                  child: Text(
                                    '⌨️ $key',
                                    style: const TextStyle(fontFamily: 'monospace'),
                                  ),
                                ))),
                              if (_pressedKeys.isNotEmpty) ...[
                                const SizedBox(height: 8),
                                TextButton(
                                  onPressed: () {
                                    setState(() {
                                      _pressedKeys.clear();
                                    });
                                  },
                                  child: const Text('清空记录'),
                                ),
                              ],
                            ],
                          ),
                        ),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 16),
                Card(
                  child: Padding(
                    padding: const EdgeInsets.all(16.0),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          '使用说明',
                          style: Theme.of(context).textTheme.titleMedium,
                        ),
                        const SizedBox(height: 8),
                        const Text('1. 点击"启用沉浸模式"按钮'),
                        const Text('2. 尝试使用Alt+Tab、Windows键等快捷键'),
                        const Text('3. 观察下方被拦截的按键记录'),
                        const Text('4. 点击"禁用沉浸模式"恢复正常'),
                        const SizedBox(height: 8),
                        const Text(
                          '注意：沉浸模式只在当前应用有焦点时生效，不会影响其他应用的正常使用。',
                          style: TextStyle(fontStyle: FontStyle.italic, color: Colors.blue),
                        ),
                      ],
                    ),
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
