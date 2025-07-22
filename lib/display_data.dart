
class DisplayData {
  final int index;
  final int width;
  final int height;
  final int refreshRate;
  final bool isVirtual;
  final String displayName;
  final String deviceName;
  final bool active;
  final int displayUid;
  
  DisplayData({
    required this.index,
    required this.width,
    required this.height,
    required this.refreshRate,
    required this.isVirtual,
    required this.displayName,
    required this.deviceName,
    required this.active,
    required this.displayUid,
  });
  
  factory DisplayData.fromMap(Map<String, dynamic> map) {
    return DisplayData(
      index: map['index'] ?? 0,
      width: map['width'] ?? 1920,
      height: map['height'] ?? 1080,
      refreshRate: map['refreshRate'] ?? 60,  // 匹配C++端的字段名
      isVirtual: map['isVirtual'] ?? false,   // 匹配C++端的字段名
      displayName: map['displayName'] ?? '',  // 匹配C++端的字段名
      deviceName: map['deviceName'] ?? '',    // 匹配C++端的字段名
      active: map['active'] ?? true,
      displayUid: map['displayUid'] ?? 0,     // 匹配C++端的字段名
    );
  }
}
