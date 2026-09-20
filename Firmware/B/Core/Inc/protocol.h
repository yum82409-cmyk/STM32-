/* protocol.h - A/B 双机 USART1 通信协议 (两机共用同一文件)
 *
 * 物理层: USART1, 115200-8-N-1, 交叉连接 (A.TX->B.RX, A.RX<-B.TX), 共地。
 * 帧格式 (定长头 + 长度 + 载荷 + XOR 校验):
 *   +------+------+------+-----+---------+---+--------+
 *   | 0xAA | 0x55 | TYPE | LEN | PAYLOAD |...| CKSUM  |
 *   +------+------+------+-----+---------+---+--------+
 *   CKSUM = TYPE ^ LEN ^ PAYLOAD[0..LEN-1]  (XOR)
 *
 * 0x01 STATE  A->B, LEN=5: [P1H P1L P2H P2L FLAGS]
 *     P1/P2 = PWM1/PWM2 占空比, 单位 0.1%, 范围 0..1000 (大端)
 *     FLAGS bit0 = 暂停标志
 * 0x02 CTRL   B->A, LEN=1: [CMD]
 *     CMD: 0x01 = 暂停, 0x00 = 恢复
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define PROT_HEAD1         0xAAU
#define PROT_HEAD2         0x55U

#define PROT_TYPE_STATE    0x01U   /* A -> B: PWM1/PWM2 占空比 + 状态 */
#define PROT_TYPE_CTRL     0x02U   /* B -> A: 暂停/恢复控制 */

#define PROT_CTRL_PAUSE    0x01U
#define PROT_CTRL_RESUME   0x00U

#define PROT_STATE_LEN     5U
#define PROT_CTRL_LEN      1U
#define PROT_MAX_PAYLOAD   8U
#define PROT_MAX_FRAME     (2U + 2U + PROT_MAX_PAYLOAD + 1U)

#define PROT_FLAG_PAUSE    0x01U

#define PROT_DUTY_MAX      1000U   /* 占空比 0.1% 单位: 1000 = 100.0% */

#endif /* PROTOCOL_H */
