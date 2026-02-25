# Android Auto Protocol Reference

*Auto-generated from AA APK v16.1.660414-release*
*80 messages, 8 enums (of 1940 total in APK)*

## Table of Contents

- [Control Channel (0)](#control) — 28 messages, 2 enums
- [Video Channel (3)](#video) — 7 messages, 5 enums
- [Input Channel (1)](#input) — 12 messages, 0 enums
- [Sensor Channel (2)](#sensor) — 17 messages, 1 enums
- [Bluetooth Channel (8)](#bluetooth) — 2 messages, 0 enums
- [WiFi Channel (14)](#wifi) — 4 messages, 0 enums
- [Navigation Channel (9)](#navigation) — 8 messages, 0 enums
- [Media Status Channel (10)](#media) — 1 messages, 0 enums
- [Phone Status Channel (11)](#phone) — 1 messages, 1 enums
- [Gearhead](#gearhead) — 2 messages, 0 enums

---

## Control Channel (0)

Session management, service discovery, auth, ping, shutdown, audio/nav focus

### HeadUnitInfo (`aacd`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | make | string |  |
| 2 | model | string |  |
| 3 | year | string |  |
| 4 | vehicle_id | string |  |
| 5 | head_unit_make | string |  |
| 6 | head_unit_model | string |  |
| 7 | head_unit_software_build | string |  |
| 8 | head_unit_software_version | string |  |

### ServiceDiscoveryRequest (`aahi`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | phone_icon_small | message | → `aahd` |
| 2 | phone_icon_medium | message | → `aagr` |
| 3 | phone_icon_large | string |  |
| 4 | device_name | string |  |
| 5 | device_brand | string |  |

### ConnectionConfiguration (`aajk`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 0 | aajh.class | uint64 |  |
| 0 | aaji.class | fixed64 |  |
| 1 | ping_configuration | message | → `zyd` |
| 2 | aajj.class | message |  |
| 4 | aajg.class | message |  |

### AVChannel (`vys`)

*Referenced by: `wbw` (ChannelDescriptor) | **seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | stream_type | enum | enum `vyn`: MEDIA_CODEC_AUDIO_PCM=1, MEDIA_CODEC_AUDIO_AAC_LC=2, MEDIA_CODEC_VIDEO_H264_BP=3, MEDIA_CODEC_AUDIO_AAC_LC_ADTS=4, MEDIA_CODEC_VIDEO_VP9=5, ...+2 |
| 2 | audio_type | enum | enum `a` |
| 3 | audio_configs | repeated message | → `vvp` |
| 4 | video_configs | repeated message | → VideoConfig (`wcz`) |
| 6 | channel_id | uint32 |  |
| 7 | display_type | enum | enum `vxg`: DISPLAY_TYPE_MAIN=0, DISPLAY_TYPE_CLUSTER=1, DISPLAY_TYPE_AUXILIARY=2 |
| 8 | keycode | enum | enum `vyh`: KEYCODE_UNKNOWN=0, KEYCODE_SOFT_LEFT=1, KEYCODE_SOFT_RIGHT=2, KEYCODE_HOME=3, KEYCODE_BACK=4, ...+267 |
| 9 | focus_reason | enum | enum `vee` |

### ChannelDescriptor (`wbw`)

*Referenced by: `wby` (ServiceDiscoveryResponse) | **seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | channel_id | **int32** (required) |  |
| 2 | sensor_channel | message | → `wbu` |
| 3 | av_channel | message | → AVChannel (`vys`) |
| 4 | input_channel | message | → `vya` |
| 5 | av_input_channel | message | → `vyt` |
| 6 | bluetooth_channel | message | → `vwc` |
| 7 | radio_channel | message | → `way` |
| 8 | navigation_channel | message | → `vzr` |
| 9 | media_infoChannel | message |  |
| 10 | phone_status_channel | message |  |
| 11 | media_browser_channel | message |  |
| 13 | notification_channel | message | → `wdh` |
| 15 | unknown_channel_15 | message |  |

### ServiceDiscoveryResponse (`wby`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | channels | repeated message | → ChannelDescriptor (`wbw`) |
| 2 | head_unit_name | string |  |
| 3 | car_model | string |  |
| 4 | car_year | string |  |
| 5 | car_serial | string |  |
| 6 | left_hand_drive_vehicle | enum | enum `vxi`: DRIVER_POSITION_LEFT=0, DRIVER_POSITION_RIGHT=1, DRIVER_POSITION_CENTER=2, DRIVER_POSITION_UNKNOWN=3 |
| 7 | headunit_manufacturer | string |  |
| 8 | headunit_model | string |  |
| 9 | sw_build | string |  |
| 10 | sw_version | string |  |
| 11 | can_play_native_media_during_vr | bool |  |
| 13 | session_configuration | int32 |  |
| 14 | display_name | string |  |
| 15 | probe_for_support | bool |  |

### BindingRequest (`wcg`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | scan_codes | repeated int32 |  |

### VendorExtensionChannel (`wcv`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | name | **string** (required) |  |
| 2 | package_white_list | repeated string |  |
| 3 | data | bytes |  |

### `aagr`

*Referenced by: `aahi` (ServiceDiscoveryRequest) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | message | → `aagh` |
| 2 | d | message | → `aafs` |
| 3 | e | message | → `aafu` |
| 4 | f | message | → `aags` |
| 5 | g | repeated message | → `aagv` |
| 7 | h | string |  |

### `aahd`

*Referenced by: `aahi` (ServiceDiscoveryRequest) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | b | string |  |
| 2 | c | bytes |  |

### `vvp`

*Referenced by: `vys` (AVChannel), `vyt` | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | **uint32** (required) |  |
| 2 | d | **uint32** (required) |  |
| 3 | e | **uint32** (required) |  |

### `vwc`

*Referenced by: `wbw` (ChannelDescriptor) | depth 1*

*(empty message)*

### `vya`

*Referenced by: `wbw` (ChannelDescriptor) | depth 1*

*(empty message)*

### `vyt`

*Referenced by: `wbw` (ChannelDescriptor) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | enum | enum `vyn`: MEDIA_CODEC_AUDIO_PCM=1, MEDIA_CODEC_AUDIO_AAC_LC=2, MEDIA_CODEC_VIDEO_H264_BP=3, MEDIA_CODEC_AUDIO_AAC_LC_ADTS=4, MEDIA_CODEC_VIDEO_VP9=5, ...+2 |
| 2 | d | message | → `vvp` |

### `vzr`

*Referenced by: `wbw` (ChannelDescriptor) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | **int32** (required) |  |
| 2 | d | **enum** (required) | enum `viz` |
| 3 | e | message | → `vzq` |

### `way`

*Referenced by: `wbw` (ChannelDescriptor) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | b | repeated message | → `wax` |

### `wbu`

*Referenced by: `wbw` (ChannelDescriptor) | depth 1*

*(empty message)*

### `wdh`

*Referenced by: `wbw` (ChannelDescriptor) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | b | string |  |

### `zyd`

*Referenced by: `aaft`, `aajk` (ConnectionConfiguration) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | b | int64 |  |
| 2 | c | int32 |  |

### `aafs`

*Referenced by: `aagr` | depth 2*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | b | string |  |
| 2 | c | string |  |
| 3 | d | string |  |
| 4 | e | string |  |

### `aafu`

*Referenced by: `aagr` | depth 2*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | message | → `aaft` |
| 3 | d | unknown_41 |  |

### `aagh`

*Referenced by: `aagr` | depth 2*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | b | bool |  |
| 2 | c | string |  |

### `aags`

*Referenced by: `aagr` | depth 2*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | b | bool |  |
| 2 | c | bool |  |

### `aagv`

*Referenced by: `aagr` | depth 2*

| # | Name | Type | Notes |
|---|------|------|-------|
| 0 | aagq.class | fixed32 |  |
| 0 | g | string |  |
| 1 | d | uint32 |  |
| 2 | e | string |  |
| 3 | h | unknown_41 |  |
| 5 | aagw.class | message |  |
| 7 | f | message |  |

### `vzq`

*Referenced by: `vzr` | depth 2*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | **int32** (required) |  |
| 2 | d | **int32** (required) |  |
| 3 | e | **int32** (required) |  |

### `wax`

*Referenced by: `way` | depth 2*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | b | **int32** (required) |  |
| 2 | c | **enum** (required) | enum `a` |
| 3 | d | repeated message | → `wbe` |
| 4 | e | repeated int32 |  |
| 5 | q | **int32** (required) |  |
| 6 | f | bool |  |
| 7 | g | enum | enum `a` |
| 8 | h | enum | enum `a` |
| 9 | i | bool |  |
| 10 | j | bool |  |
| 11 | k | enum | enum `a` |
| 13 | n | bool |  |

### `aaft`

*Referenced by: `aafu` | depth 3*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | message | → `zyd` |
| 2 | d | message | → `zyd` |

### `wbe`

*Referenced by: `wax` | depth 3*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | b | **int32** (required) |  |
| 2 | c | **int32** (required) |  |

### Enums (control)

**`vxi`**
*Used by: `wby`*

- `DRIVER_POSITION_LEFT` = 0
- `DRIVER_POSITION_RIGHT` = 1
- `DRIVER_POSITION_CENTER` = 2
- `DRIVER_POSITION_UNKNOWN` = 3

**`vyn`**
*Used by: `vys`, `vyt`, `wcz`*

- `MEDIA_CODEC_AUDIO_PCM` = 1
- `MEDIA_CODEC_AUDIO_AAC_LC` = 2
- `MEDIA_CODEC_VIDEO_H264_BP` = 3
- `MEDIA_CODEC_AUDIO_AAC_LC_ADTS` = 4
- `MEDIA_CODEC_VIDEO_VP9` = 5
- `MEDIA_CODEC_VIDEO_AV1` = 6
- `MEDIA_CODEC_VIDEO_H265` = 7

---

## Video Channel (3)

Video streaming config, codec negotiation, focus management, AV setup

### AVChannelSetupResponse (`vxb`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | media_status | **enum** (required) | enum `viz` |
| 2 | max_unacked | uint32 |  |
| 3 | configs | repeated uint32 |  |

### AVChannel (`vys`)

*Referenced by: `wbw` (ChannelDescriptor) | **seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | stream_type | enum | enum `vyn`: MEDIA_CODEC_AUDIO_PCM=1, MEDIA_CODEC_AUDIO_AAC_LC=2, MEDIA_CODEC_VIDEO_H264_BP=3, MEDIA_CODEC_AUDIO_AAC_LC_ADTS=4, MEDIA_CODEC_VIDEO_VP9=5, ...+2 |
| 2 | audio_type | enum | enum `a` |
| 3 | audio_configs | repeated message | → `vvp` |
| 4 | video_configs | repeated message | → VideoConfig (`wcz`) |
| 6 | channel_id | uint32 |  |
| 7 | display_type | enum | enum `vxg`: DISPLAY_TYPE_MAIN=0, DISPLAY_TYPE_CLUSTER=1, DISPLAY_TYPE_AUXILIARY=2 |
| 8 | keycode | enum | enum `vyh`: KEYCODE_UNKNOWN=0, KEYCODE_SOFT_LEFT=1, KEYCODE_SOFT_RIGHT=2, KEYCODE_HOME=3, KEYCODE_BACK=4, ...+267 |
| 9 | focus_reason | enum | enum `vee` |

### AVInputOpenRequest (`vyx`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | open | **bool** (required) |  |
| 2 | anc | bool |  |
| 3 | ec | bool |  |
| 4 | max_unacked | int32 |  |

### VideoConfig (`wcz`)

*Referenced by: `vys` (AVChannel) | **seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | video_resolution | enum | enum `wcy`: VIDEO_800x480=1, VIDEO_1280x720=2, VIDEO_1920x1080=3, VIDEO_2560x1440=4, VIDEO_3840x2160=5, ...+4 |
| 2 | video_fps | enum | enum `viz` |
| 3 | margin_width | uint32 |  |
| 4 | margin_height | uint32 |  |
| 5 | dpi | uint32 |  |
| 6 | additional_depth | uint32 |  |
| 7 | field7 | uint32 |  |
| 8 | field8 | uint32 |  |
| 9 | field9 | uint32 |  |
| 10 | codec | enum | enum `vyn`: MEDIA_CODEC_AUDIO_PCM=1, MEDIA_CODEC_AUDIO_AAC_LC=2, MEDIA_CODEC_VIDEO_H264_BP=3, MEDIA_CODEC_AUDIO_AAC_LC_ADTS=4, MEDIA_CODEC_VIDEO_VP9=5, ...+2 |
| 11 | additional_config | message | → `wcm` |

### VideoFocusIndication (`wdb`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | focus_mode | enum | enum `wda`: VIDEO_FOCUS_PROJECTED=1, VIDEO_FOCUS_NATIVE=2, VIDEO_FOCUS_NATIVE_TRANSIENT=3, VIDEO_FOCUS_PROJECTED_NO_INPUT_FOCUS=4 |
| 2 | unrequested | bool |  |

### `vvp`

*Referenced by: `vys` (AVChannel), `vyt` | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | **uint32** (required) |  |
| 2 | d | **uint32** (required) |  |
| 3 | e | **uint32** (required) |  |

### `wcm`

*Referenced by: `wcz` (VideoConfig) | depth 1*

*(empty message)*

### Enums (video)

**`vxg`**
*Used by: `vys`*

- `DISPLAY_TYPE_MAIN` = 0
- `DISPLAY_TYPE_CLUSTER` = 1
- `DISPLAY_TYPE_AUXILIARY` = 2

**`vyh`**
*Used by: `vys`*

- `KEYCODE_UNKNOWN` = 0
- `KEYCODE_SOFT_LEFT` = 1
- `KEYCODE_SOFT_RIGHT` = 2
- `KEYCODE_HOME` = 3
- `KEYCODE_BACK` = 4
- `KEYCODE_CALL` = 5
- `KEYCODE_ENDCALL` = 6
- `KEYCODE_0` = 7
- `KEYCODE_1` = 8
- `KEYCODE_2` = 9
- `KEYCODE_3` = 10
- `KEYCODE_4` = 11
- `KEYCODE_5` = 12
- `KEYCODE_6` = 13
- `KEYCODE_7` = 14
- `KEYCODE_8` = 15
- `KEYCODE_9` = 16
- `KEYCODE_STAR` = 17
- `KEYCODE_POUND` = 18
- `KEYCODE_DPAD_UP` = 19
- `KEYCODE_DPAD_DOWN` = 20
- `KEYCODE_DPAD_LEFT` = 21
- `KEYCODE_DPAD_RIGHT` = 22
- `KEYCODE_DPAD_CENTER` = 23
- `KEYCODE_VOLUME_UP` = 24
- `KEYCODE_VOLUME_DOWN` = 25
- `KEYCODE_POWER` = 26
- `KEYCODE_CAMERA` = 27
- `KEYCODE_CLEAR` = 28
- `KEYCODE_A` = 29
- `KEYCODE_B` = 30
- `KEYCODE_C` = 31
- `KEYCODE_D` = 32
- `KEYCODE_E` = 33
- `KEYCODE_F` = 34
- `KEYCODE_G` = 35
- `KEYCODE_H` = 36
- `KEYCODE_I` = 37
- `KEYCODE_J` = 38
- `KEYCODE_K` = 39
- `KEYCODE_L` = 40
- `KEYCODE_M` = 41
- `KEYCODE_N` = 42
- `KEYCODE_O` = 43
- `KEYCODE_P` = 44
- `KEYCODE_Q` = 45
- `KEYCODE_R` = 46
- `KEYCODE_S` = 47
- `KEYCODE_T` = 48
- `KEYCODE_U` = 49
- `KEYCODE_V` = 50
- `KEYCODE_W` = 51
- `KEYCODE_X` = 52
- `KEYCODE_Y` = 53
- `KEYCODE_Z` = 54
- `KEYCODE_COMMA` = 55
- `KEYCODE_PERIOD` = 56
- `KEYCODE_ALT_LEFT` = 57
- `KEYCODE_ALT_RIGHT` = 58
- `KEYCODE_SHIFT_LEFT` = 59
- `KEYCODE_SHIFT_RIGHT` = 60
- `KEYCODE_TAB` = 61
- `KEYCODE_SPACE` = 62
- `KEYCODE_SYM` = 63
- `KEYCODE_EXPLORER` = 64
- `KEYCODE_ENVELOPE` = 65
- `KEYCODE_ENTER` = 66
- `KEYCODE_DEL` = 67
- `KEYCODE_GRAVE` = 68
- `KEYCODE_MINUS` = 69
- `KEYCODE_EQUALS` = 70
- `KEYCODE_LEFT_BRACKET` = 71
- `KEYCODE_RIGHT_BRACKET` = 72
- `KEYCODE_BACKSLASH` = 73
- `KEYCODE_SEMICOLON` = 74
- `KEYCODE_APOSTROPHE` = 75
- `KEYCODE_SLASH` = 76
- `KEYCODE_AT` = 77
- `KEYCODE_NUM` = 78
- `KEYCODE_HEADSETHOOK` = 79
- `KEYCODE_FOCUS` = 80
- `KEYCODE_PLUS` = 81
- `KEYCODE_MENU` = 82
- `KEYCODE_NOTIFICATION` = 83
- `KEYCODE_SEARCH` = 84
- `KEYCODE_MEDIA_PLAY_PAUSE` = 85
- `KEYCODE_MEDIA_STOP` = 86
- `KEYCODE_MEDIA_NEXT` = 87
- `KEYCODE_MEDIA_PREVIOUS` = 88
- `KEYCODE_MEDIA_REWIND` = 89
- `KEYCODE_MEDIA_FAST_FORWARD` = 90
- `KEYCODE_MUTE` = 91
- `KEYCODE_PAGE_UP` = 92
- `KEYCODE_PAGE_DOWN` = 93
- `KEYCODE_PICTSYMBOLS` = 94
- `KEYCODE_SWITCH_CHARSET` = 95
- `KEYCODE_BUTTON_A` = 96
- `KEYCODE_BUTTON_B` = 97
- `KEYCODE_BUTTON_C` = 98
- `KEYCODE_BUTTON_X` = 99
- `KEYCODE_BUTTON_Y` = 100
- `KEYCODE_BUTTON_L1` = 102
- `KEYCODE_BUTTON_R1` = 103
- `KEYCODE_BUTTON_L2` = 104
- `KEYCODE_BUTTON_R2` = 105
- `KEYCODE_BUTTON_THUMBL` = 106
- `KEYCODE_BUTTON_THUMBR` = 107
- `KEYCODE_BUTTON_START` = 108
- `KEYCODE_BUTTON_SELECT` = 109
- `KEYCODE_BUTTON_MODE` = 110
- `KEYCODE_ESCAPE` = 111
- `KEYCODE_FORWARD_DEL` = 112
- `KEYCODE_CTRL_LEFT` = 113
- `KEYCODE_CTRL_RIGHT` = 114
- `KEYCODE_CAPS_LOCK` = 115
- `KEYCODE_SCROLL_LOCK` = 116
- `KEYCODE_META_LEFT` = 117
- `KEYCODE_META_RIGHT` = 118
- `KEYCODE_FUNCTION` = 119
- `KEYCODE_SYSRQ` = 120
- `KEYCODE_BREAK` = 121
- `KEYCODE_MOVE_HOME` = 122
- `KEYCODE_MOVE_END` = 123
- `KEYCODE_INSERT` = 124
- `KEYCODE_FORWARD` = 125
- `KEYCODE_MEDIA_PLAY` = 126
- `KEYCODE_MEDIA_PAUSE` = 127
- `KEYCODE_MEDIA_CLOSE` = 128
- `KEYCODE_MEDIA_EJECT` = 129
- `KEYCODE_MEDIA_RECORD` = 130
- `KEYCODE_F1` = 131
- `KEYCODE_F2` = 132
- `KEYCODE_F3` = 133
- `KEYCODE_F4` = 134
- `KEYCODE_F5` = 135
- `KEYCODE_F6` = 136
- `KEYCODE_F7` = 137
- `KEYCODE_F8` = 138
- `KEYCODE_F9` = 139
- `KEYCODE_F10` = 140
- `KEYCODE_F11` = 141
- `KEYCODE_F12` = 142
- `KEYCODE_NUM_LOCK` = 143
- `KEYCODE_NUMPAD_0` = 144
- `KEYCODE_NUMPAD_1` = 145
- `KEYCODE_NUMPAD_2` = 146
- `KEYCODE_NUMPAD_3` = 147
- `KEYCODE_NUMPAD_4` = 148
- `KEYCODE_NUMPAD_5` = 149
- `KEYCODE_NUMPAD_6` = 150
- `KEYCODE_NUMPAD_7` = 151
- `KEYCODE_NUMPAD_8` = 152
- `KEYCODE_NUMPAD_9` = 153
- `KEYCODE_NUMPAD_DIVIDE` = 154
- `KEYCODE_NUMPAD_MULTIPLY` = 155
- `KEYCODE_NUMPAD_SUBTRACT` = 156
- `KEYCODE_NUMPAD_ADD` = 157
- `KEYCODE_NUMPAD_DOT` = 158
- `KEYCODE_NUMPAD_COMMA` = 159
- `KEYCODE_NUMPAD_ENTER` = 160
- `KEYCODE_NUMPAD_EQUALS` = 161
- `KEYCODE_NUMPAD_LEFT_PAREN` = 162
- `KEYCODE_NUMPAD_RIGHT_PAREN` = 163
- `KEYCODE_VOLUME_MUTE` = 164
- `KEYCODE_INFO` = 165
- `KEYCODE_CHANNEL_UP` = 166
- `KEYCODE_CHANNEL_DOWN` = 167
- `KEYCODE_ZOOM_IN` = 168
- `KEYCODE_ZOOM_OUT` = 169
- `KEYCODE_TV` = 170
- `KEYCODE_WINDOW` = 171
- `KEYCODE_GUIDE` = 172
- `KEYCODE_DVR` = 173
- `KEYCODE_BOOKMARK` = 174
- `KEYCODE_CAPTIONS` = 175
- `KEYCODE_SETTINGS` = 176
- `KEYCODE_TV_POWER` = 177
- `KEYCODE_TV_INPUT` = 178
- `KEYCODE_STB_POWER` = 179
- `KEYCODE_STB_INPUT` = 180
- `KEYCODE_AVR_POWER` = 181
- `KEYCODE_AVR_INPUT` = 182
- `KEYCODE_PROG_RED` = 183
- `KEYCODE_PROG_GREEN` = 184
- `KEYCODE_PROG_YELLOW` = 185
- `KEYCODE_PROG_BLUE` = 186
- `KEYCODE_APP_SWITCH` = 187
- `KEYCODE_BUTTON_1` = 188
- `KEYCODE_BUTTON_2` = 189
- `KEYCODE_BUTTON_3` = 190
- `KEYCODE_BUTTON_4` = 191
- `KEYCODE_BUTTON_5` = 192
- `KEYCODE_BUTTON_6` = 193
- `KEYCODE_BUTTON_7` = 194
- `KEYCODE_BUTTON_8` = 195
- `KEYCODE_BUTTON_9` = 196
- `KEYCODE_BUTTON_10` = 197
- `KEYCODE_BUTTON_11` = 198
- `KEYCODE_BUTTON_12` = 199
- `KEYCODE_BUTTON_13` = 200
- `KEYCODE_BUTTON_14` = 201
- `KEYCODE_BUTTON_15` = 202
- `KEYCODE_BUTTON_16` = 203
- `KEYCODE_LANGUAGE_SWITCH` = 204
- `KEYCODE_MANNER_MODE` = 205
- `KEYCODE_3D_MODE` = 206
- `KEYCODE_CONTACTS` = 207
- `KEYCODE_CALENDAR` = 208
- `KEYCODE_MUSIC` = 209
- `KEYCODE_CALCULATOR` = 210
- `KEYCODE_ZENKAKU_HANKAKU` = 211
- `KEYCODE_EISU` = 212
- `KEYCODE_MUHENKAN` = 213
- `KEYCODE_HENKAN` = 214
- `KEYCODE_KATAKANA_HIRAGANA` = 215
- `KEYCODE_YEN` = 216
- `KEYCODE_RO` = 217
- `KEYCODE_KANA` = 218
- `KEYCODE_ASSIST` = 219
- `KEYCODE_BRIGHTNESS_DOWN` = 220
- `KEYCODE_BRIGHTNESS_UP` = 221
- `KEYCODE_MEDIA_AUDIO_TRACK` = 222
- `KEYCODE_SLEEP` = 223
- `KEYCODE_WAKEUP` = 224
- `KEYCODE_PAIRING` = 225
- `KEYCODE_MEDIA_TOP_MENU` = 226
- `KEYCODE_11` = 227
- `KEYCODE_12` = 228
- `KEYCODE_LAST_CHANNEL` = 229
- `KEYCODE_TV_DATA_SERVICE` = 230
- `KEYCODE_VOICE_ASSIST` = 231
- `KEYCODE_TV_RADIO_SERVICE` = 232
- `KEYCODE_TV_TELETEXT` = 233
- `KEYCODE_TV_NUMBER_ENTRY` = 234
- `KEYCODE_TV_TERRESTRIAL_ANALOG` = 235
- `KEYCODE_TV_TERRESTRIAL_DIGITAL` = 236
- `KEYCODE_TV_SATELLITE` = 237
- `KEYCODE_TV_SATELLITE_BS` = 238
- `KEYCODE_TV_SATELLITE_CS` = 239
- `KEYCODE_TV_SATELLITE_SERVICE` = 240
- `KEYCODE_TV_NETWORK` = 241
- `KEYCODE_TV_ANTENNA_CABLE` = 242
- `KEYCODE_TV_INPUT_HDMI_1` = 243
- `KEYCODE_TV_INPUT_HDMI_2` = 244
- `KEYCODE_TV_INPUT_HDMI_3` = 245
- `KEYCODE_TV_INPUT_HDMI_4` = 246
- `KEYCODE_TV_INPUT_COMPOSITE_1` = 247
- `KEYCODE_TV_INPUT_COMPOSITE_2` = 248
- `KEYCODE_TV_INPUT_COMPONENT_1` = 249
- `KEYCODE_TV_INPUT_COMPONENT_2` = 250
- `KEYCODE_TV_INPUT_VGA_1` = 251
- `KEYCODE_TV_AUDIO_DESCRIPTION` = 252
- `KEYCODE_TV_AUDIO_DESCRIPTION_MIX_UP` = 253
- `KEYCODE_TV_AUDIO_DESCRIPTION_MIX_DOWN` = 254
- `KEYCODE_TV_ZOOM_MODE` = 255
- `KEYCODE_TV_CONTENTS_MENU` = 256
- `KEYCODE_TV_MEDIA_CONTEXT_MENU` = 257
- `KEYCODE_TV_TIMER_PROGRAMMING` = 258
- `KEYCODE_HELP` = 259
- `KEYCODE_NAVIGATE_PREVIOUS` = 260
- `KEYCODE_NAVIGATE_NEXT` = 261
- `KEYCODE_NAVIGATE_IN` = 262
- `KEYCODE_NAVIGATE_OUT` = 263
- `KEYCODE_DPAD_UP_LEFT` = 268
- `KEYCODE_DPAD_DOWN_LEFT` = 269
- `KEYCODE_DPAD_UP_RIGHT` = 270
- `KEYCODE_DPAD_DOWN_RIGHT` = 271
- `KEYCODE_SENTINEL` = 65535
- `KEYCODE_ROTARY_CONTROLLER` = 65536
- `KEYCODE_MEDIA` = 65537
- `KEYCODE_TERTIARY_BUTTON` = 65543
- `KEYCODE_TURN_CARD` = 65544

**`vyn`**
*Used by: `vys`, `vyt`, `wcz`*

- `MEDIA_CODEC_AUDIO_PCM` = 1
- `MEDIA_CODEC_AUDIO_AAC_LC` = 2
- `MEDIA_CODEC_VIDEO_H264_BP` = 3
- `MEDIA_CODEC_AUDIO_AAC_LC_ADTS` = 4
- `MEDIA_CODEC_VIDEO_VP9` = 5
- `MEDIA_CODEC_VIDEO_AV1` = 6
- `MEDIA_CODEC_VIDEO_H265` = 7

**`wcy`**
*Used by: `wcz`*

- `VIDEO_800x480` = 1
- `VIDEO_1280x720` = 2
- `VIDEO_1920x1080` = 3
- `VIDEO_2560x1440` = 4
- `VIDEO_3840x2160` = 5
- `VIDEO_720x1280` = 6
- `VIDEO_1080x1920` = 7
- `VIDEO_1440x2560` = 8
- `VIDEO_2160x3840` = 9

**`wda`**
*Used by: `wdb`*

- `VIDEO_FOCUS_PROJECTED` = 1
- `VIDEO_FOCUS_NATIVE` = 2
- `VIDEO_FOCUS_NATIVE_TRANSIENT` = 3
- `VIDEO_FOCUS_PROJECTED_NO_INPUT_FOCUS` = 4

---

## Input Channel (1)

Touch events, button events, relative/absolute input

### InputEventIndication (`vxx`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | timestamp | **uint64** (required) |  |
| 3 | touch_event | message | → TouchEvent (`wcj`) |
| 4 | button_event | message | → `vyj` |
| 5 | absolute_input_event | message | → `vvi` |
| 6 | relative_input_event | message | → `wbm` |
| 7 | secondary_touch_event | message | → TouchEvent (`wcj`) |

### ButtonEvent (`vyi`)

*Referenced by: `vyj` | **seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | scan_code | **uint32** (required) |  |
| 2 | is_pressed | **bool** (required) |  |
| 3 | meta | **uint32** (required) |  |
| 4 | long_press | bool |  |

### TouchEvent (`wcj`)

*Referenced by: `vxx` (InputEventIndication) | **seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | touch_location | repeated message | → `wci` |
| 2 | action_index | uint32 |  |
| 3 | touch_action | enum | enum `vee` |

### InputChannel (`zpl`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | supported_keycodes | message | → `zpn` |
| 2 | touch_screen_config | message | → `zpn` |
| 3 | touch_pad_config | message | → `zpn` |

### `vvi`

*Referenced by: `vxx` (InputEventIndication) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | b | repeated message | → `vvh` |

### `vyj`

*Referenced by: `vxx` (InputEventIndication) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | b | repeated message | → ButtonEvent (`vyi`) |

### `wbm`

*Referenced by: `vxx` (InputEventIndication) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | b | repeated message | → `wbl` |

### `wci`

*Referenced by: `wcj` (TouchEvent) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | **uint32** (required) |  |
| 2 | d | **uint32** (required) |  |
| 3 | e | **uint32** (required) |  |

### `zpn`

*Referenced by: `zpl` (InputChannel) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | int32 |  |
| 2 | d | message | → `zli` |

### `vvh`

*Referenced by: `vvi` | depth 2*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | b | **uint32** (required) |  |
| 2 | c | **int32** (required) |  |

### `wbl`

*Referenced by: `wbm` | depth 2*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | **uint32** (required) |  |
| 2 | d | **int32** (required) |  |

### `zli`

*Referenced by: `zpn` | depth 2*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | b | double |  |

---

## Sensor Channel (2)

GPS, compass, speed, RPM, night mode, driving status, diagnostics

### GPSLocation (`ahdz`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | timestamp | int32 |  |
| 2 | latitude | int32 |  |
| 3 | longitude | int32 |  |
| 4 | accuracy | int32 |  |
| 5 | altitude | int32 |  |
| 6 | speed | int32 |  |
| 7 | bearing | int32 |  |
| 8 | pdop | int32 |  |
| 9 | hdop | int32 |  |
| 11 | vdop | int32 |  |
| 14 | satellites_used | int32 |  |
| 15 | fix_type | int32 |  |
| 16 | dgps_age | int32 |  |
| 17 | dgps_station | int32 |  |
| 18 | magnetic_variation | int32 |  |
| 20 | utc_time | int64 |  |
| 21 | elapsed_realtime | int64 |  |
| 22 | elapsed_realtime_uncertainty | int64 |  |

### Door (`vxh`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | hood_open | bool |  |
| 2 | boot_open | bool |  |
| 3 | door_open | repeated bool |  |

### Light (`vyk`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | headlight | enum | enum `a` |
| 2 | indicator | enum | enum `a` |
| 3 | hazard_light_on | bool |  |

### SensorStartRequest (`war`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | enum | enum `waq`: IDENTIFIER_TYPE_INVALID=0, IDENTIFIER_TYPE_AMFM_FREQUENCY=1, IDENTIFIER_TYPE_RDS_PI=2, IDENTIFIER_TYPE_HD_STATION_ID_EXT=3, IDENTIFIER_TYPE_HD_STATION_NAME=4, ...+5 |
| 2 | d | uint64 |  |

### SensorEventIndication (`wbo`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | gps_location | repeated message | → `vyl` |
| 2 | compass | repeated message | → `vxa` |
| 3 | speed | repeated message | → `wcd` |
| 4 | rpm | repeated message | → `wbn` |
| 5 | odometer | repeated message | → `vzx` |
| 6 | fuel_level | repeated message | → `vxn` |
| 7 | parking_brake | repeated message | → `wab` |
| 8 | gear | repeated message | → `vxp` |
| 9 | diagnostics | repeated message | → `vxf` |
| 10 | night_mode | repeated message | → `vzw` |
| 11 | enviorment | repeated message | → `vxk` |
| 27 | extended_sensor_data | sfixed32 |  |
| 27 | extended_sensor_data | sint32 |  |
| 27 | extended_sensor_data | sint64 |  |
| 27 | extended_sensor_data | group |  |
| 27 | extended_sensor_data | repeated double |  |
| 27 | extended_sensor_data | repeated float |  |
| 27 | extended_sensor_data | repeated int64 |  |
| 27 | extended_sensor_data | repeated uint64 |  |
| 27 | extended_sensor_data | repeated fixed32 |  |
| 27 | extended_sensor_data | repeated bool |  |
| 27 | extended_sensor_data | repeated string |  |

### Diagnostics (`xss`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | diagnostics | bytes |  |
| 2 | extended_diagnostics | repeated bytes |  |
| 3 | diagnostics_supported | bool |  |

### `vxa`

*Referenced by: `wbo` (SensorEventIndication) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | **int32** (required) |  |
| 2 | d | int32 |  |
| 3 | e | int32 |  |

### `vxf`

*Referenced by: `wbo` (SensorEventIndication) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | bytes |  |

### `vxk`

*Referenced by: `wbo` (SensorEventIndication) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | int32 |  |
| 2 | d | int32 |  |
| 3 | e | int32 |  |

### `vxn`

*Referenced by: `wbo` (SensorEventIndication) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | int32 |  |
| 2 | d | int32 |  |
| 3 | e | bool |  |

### `vxp`

*Referenced by: `wbo` (SensorEventIndication) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | b | **enum** (required) | enum `vee` |

### `vyl`

*Referenced by: `wbo` (SensorEventIndication) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 2 | c | **int32** (required) |  |
| 3 | d | **int32** (required) |  |
| 4 | e | uint32 |  |
| 5 | f | int32 |  |
| 6 | g | int32 |  |
| 7 | h | int32 |  |

### `vzw`

*Referenced by: `wbo` (SensorEventIndication) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | bool |  |

### `vzx`

*Referenced by: `wbo` (SensorEventIndication) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | **int32** (required) |  |
| 2 | d | int32 |  |

### `wab`

*Referenced by: `wbo` (SensorEventIndication) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | b | **bool** (required) |  |

### `wbn`

*Referenced by: `wbo` (SensorEventIndication) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | b | **int32** (required) |  |

### `wcd`

*Referenced by: `wbo` (SensorEventIndication) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | **int32** (required) |  |
| 2 | d | bool |  |
| 4 | e | int32 |  |

### Enums (sensor)

**`waq`**
*Used by: `war`*

- `IDENTIFIER_TYPE_INVALID` = 0
- `IDENTIFIER_TYPE_AMFM_FREQUENCY` = 1
- `IDENTIFIER_TYPE_RDS_PI` = 2
- `IDENTIFIER_TYPE_HD_STATION_ID_EXT` = 3
- `IDENTIFIER_TYPE_HD_STATION_NAME` = 4
- `IDENTIFIER_TYPE_DAB_DMB_SID_EXT` = 5
- `IDENTIFIER_TYPE_DAB_ENSEMBLE` = 6
- `IDENTIFIER_TYPE_DAB_SCID` = 7
- `IDENTIFIER_TYPE_DAB_FREQUENCY` = 8
- `IDENTIFIER_TYPE_DAB_SID_EXT` = 9

---

## Bluetooth Channel (8)

BT pairing requests/responses

### BluetoothPairingRequest (`kay`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | phone_address | string |  |
| 2 | pairing_method | enum |  |
| 3 | phone_name | string |  |

### BluetoothPairingResponse (`xgq`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | already_paired | bool |  |
| 2 | status | enum |  |
| 3 | error_message | string |  |

---

## WiFi Channel (14)

WiFi credential exchange for wireless AA

### WifiSecurityResponse (`wan`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | ssid | string |  |
| 2 | key | string |  |
| 3 | bssid | string |  |
| 4 | security_mode | message | → `waw` |
| 5 | access_point_type | message | → `wam` |
| 6 | wifi_direct_config | message | → `wbb` |
| 7 | ip_address | string |  |
| 8 | gateway | string |  |
| 9 | prefix_length | uint32 |  |
| 10 | hidden_network | bool |  |
| 11 | band_5ghz | bool |  |

### `wam`

*Referenced by: `wan` (WifiSecurityResponse), `wbb` | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | bytes |  |

### `waw`

*Referenced by: `wan` (WifiSecurityResponse) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | b | enum | enum `viz` |
| 2 | c | uint32 |  |

### `wbb`

*Referenced by: `wan` (WifiSecurityResponse) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | string |  |
| 2 | d | string |  |
| 3 | e | string |  |
| 4 | f | string |  |
| 5 | g | message | → `wam` |
| 6 | h | uint64 |  |

---

## Navigation Channel (9)

Turn-by-turn steps, distance/ETA, navigation state

### NavigationChannel (`ahdp`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | minimum_interval_ms | int32 |  |
| 2 | type | int32 |  |
| 3 | image_options | message | → `ahdl` |

### NavigationStep (`utj`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | address | string |  |
| 2 | icon | bytes |  |
| 3 | e | string |  |
| 4 | cue | int64 |  |
| 5 | g | repeated message | → `utk` |

### NavigationDistance (`xnb`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | distance | **message** (required) | → `xmw` |
| 2 | value | **string** (required) |  |
| 3 | unit | **int32** (required) |  |
| 8 | f | message | → `xng` |

### `ahdl`

*Referenced by: `ahdp` (NavigationChannel) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | fixed64 |  |
| 2 | d | string |  |

### `utk`

*Referenced by: `utj` (NavigationStep) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 0 |  | uint64 |  |
| 0 |  | fixed64 |  |
| 1 | e | string |  |
| 2 |  | int64 |  |
| 4 |  | double |  |

### `xmw`

*Referenced by: `xnb` (NavigationDistance) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | **int64** (required) |  |
| 2 | d | **fixed32** (required) |  |
| 3 | e | **fixed32** (required) |  |

### `xng`

*Referenced by: `xnb` (NavigationDistance) | depth 1*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | e | **message** (required) | → `xnd` |
| 2 | f | repeated message | → `xnd` |
| 4 | xne.class | message |  |

### `xnd`

*Referenced by: `xng` | depth 2*

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | **string** (required) |  |
| 2 | d | string |  |
| 3 | e | int64 |  |
| 4 | f | sfixed32 |  |

---

## Media Status Channel (10)

Playback state, track metadata, album art

### MediaPlaybackMetadata (`nmi`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | title | string |  |
| 2 | artist | string |  |
| 3 | album | string |  |
| 4 | album_art | bytes |  |
| 5 | is_playing | bool |  |
| 6 | album_art_url | string |  |

---

## Phone Status Channel (11)

Call state, signal strength, battery

### PhoneStatus (`wad`)

***seed***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | **enum** (required) | enum `wae`: UNKNOWN=0, IN_CALL=1, ON_HOLD=2, INACTIVE=3, INCOMING=4, ...+2 |
| 2 | d | **uint32** (required) |  |
| 3 | e | string |  |
| 4 | f | string |  |
| 5 | g | string |  |
| 6 | h | bytes |  |

### Enums (phone)

**`wae`**
*Used by: `wad`*

- `UNKNOWN` = 0
- `IN_CALL` = 1
- `ON_HOLD` = 2
- `INACTIVE` = 3
- `INCOMING` = 4
- `CONFERENCED` = 5
- `MUTED` = 6

---

## Gearhead

### `nll`

***gearhead root***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | bool |  |
| 2 | d | bool |  |
| 3 | e | bool |  |
| 4 | f | bool |  |
| 5 | g | bool |  |
| 6 | h | bool |  |
| 7 | i | bool |  |
| 8 | j | bool |  |
| 9 | k | bool |  |
| 10 | l | bool |  |
| 11 | m | bool |  |
| 13 | p | bool |  |

### `nlq`

***gearhead root***

| # | Name | Type | Notes |
|---|------|------|-------|
| 1 | c | bool |  |
| 2 | d | bool |  |
| 3 | e | bool |  |
| 5 | f | bool |  |
| 6 | g | bool |  |
| 7 | h | bool |  |
| 8 | i | bool |  |
| 10 | j | bool |  |
| 11 | k | bool |  |
| 13 | m | bool |  |
