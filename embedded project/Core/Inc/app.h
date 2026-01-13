//
// Created by link on 2025/12/5.
//

#ifndef G070C_APP_H
#define G070C_APP_H

#ifdef __cplusplus
extern "C" {
#endif
#include "main.h"
// 标准的GGA 语句最长72个字符
typedef struct
{
  double latitude;       // 纬度
  double longitude;      // 经度
  float altitude;        // 高度
  float heading;         // 航向角
  float velN;            // 北向速度
  float velE;            // 东向速度
  float velD;            // 下向速度
  uint8_t fixType;       // 0=无效,2=DGPS,4=RTK固定,5=RTK浮点
  uint8_t satellites;    // 卫星数量
  uint32_t utc_sec;      // UTC秒
  uint32_t usec;         // PPS微秒计数
} RTKData_t;

typedef struct
{
  uint8_t beaconId;       // 发射端 ID：如 20 / 21 / 22
  float   distance;       // 距离（m），失败时 = -110
  float   delay;          // 传播时延（s）
  float   snr;            // 信噪比（dB）
  uint8_t valid;          // 1=有效 0=无效
} AcousticRange_t;

typedef struct
{
  uint8_t receiverId;      // 接收端 ID，如 10
  AcousticRange_t range[3]; // 20、21、22 三个声波发射信标的结果
  uint16_t seq;            // 序号，如 471
  uint32_t timestamp;      // 接收时间戳
} AcousticData_t;

typedef struct
{
  uint8_t id;    // 基站 ID, 0, 1, 2, 3 基站0（接收端ID10）
  uint32_t timestamp;

  RTKData_t rtk;
  AcousticData_t data;

} StationData_t;


void app_init(void);

void app_task(void);


#ifdef __cplusplus
}
#endif

#endif //G070C_APP_H
