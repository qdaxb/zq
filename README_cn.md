# Zindex

## 介绍
受开源版[zindex](https://github.com/mattgodbolt/zindex)工具的启发，重写部分功能后后得到的gzip日志检索工具。

主要修改了开源版的以下方面：
1. 重写全部索引逻辑，降低索引文件尺寸，开源版索引文件尺寸与日志归档文件尺寸比例约1:1.3，经过优化后尺寸比例为50:1
2. 删除部分参数及功能，开源版需要通过config file支持多列索引，修改后可通过参数直接指定多列

## 能力
### 索引文件尺寸
索引尺寸与待索引字段的数据分布及文件行数相关，字段内容重复率高则索引文件体积降低。

测试一个压缩后2GB的700万行无重复字段的日志文件，产生的索引文件约为36MB.

### 检索性能

检索性能与待检索字段的数据分布及文件大小相关，字段重复率高则检索速度变慢。

同上文件，直接zgrep耗时约30+s，通过索引检索时间约0.4s。

## 原理
### 索引
#### chunk索引
gzip文件中seek功能基于xlib作者的zrand.c文件开发，大致逻辑为将gzip文件按尺寸拆分为chunk，每个chunk需要创建一个32kb的dict索引，之后可以直接seek到该chunk起始位置并开始解压缩。具体逻辑可以参考https://github.com/circulosmeos/gztool background部分
zindex工程的chunk索引保留了开源版大部分逻辑，默认每32MB创建一个chunk，chunk的dict内容保存在sqllite数据库中，跳转到chunk后的内部索引通过解压缩并drop数据实现。

#### field索引
对待索引field做hash32后取模(0xFFFFu)作为索引key，内容所在行对应的gzip offset作为索引value。整体采用hash<key,list<value>>方式组织索引数据。
为节约存储，采用二进制原始数据方式组织文件，文件格式为：
```
key  | size  | offset1 | offset2 | offset3
4byte| 4byte | 8byte   | 8byte   | 8byte
```

查找文件时，先读取key及size，如果key不匹配则继续读取`size*8`个空间并drop掉，然后继续读取下一个key。

这里为降低数据存储空间，采用了压缩方式保存offset，由于每个key对应的offset为一个数组，在offset超过0xFFFFFFFE时，将增加一个特殊mask占位符，扫描到mask之后,后续的offset值会做如下计算`offset=offset+0xFFFFFFFE*count(mask)`

同时，由于每个list的空间不再是定长存储，无法精确drop list，因此增加一个padding占位(`0xFFFFFFFF`)，每次key不匹配则drop `size*4`个空间，然后继续读到`0xFFFFFFFF`。

最终的索引文件格式为
```
padding | key   | size  | offset1 | offset2 | mask  | offset3
4byte   | 4byte | 4byte | 4byte   | 4byte   | 4byte | 4byte
```

已知问题：
1. 每次查询要顺序读取全部文件，由于索引文件较小，因此这部分时间代价不高。如果考虑继续优化，可以在生成索引时将key放在文件头部，并增加对应value list的offset
2. 索引文件尺寸与key的数量及offset list的平均长度相关，若key太多，则每key需要额外增加4byte padding，并且offset mask无法降低存储；若key太少，则冲突概率变高，offset list的value数量增加，需要解压大量chunk获取数据。目前key的数量最大为65536个，offset list数量不限制。后续可以考虑根据文件内容情况自动判断key及list大小。

## 使用方法

### zindex - 为压缩文件创建索引
```
用法: zindex [选项] 输入文件

选项:
  -v, --verbose              显示更详细信息
      --debug                显示调试信息
      --colour, --color      强制使用彩色输出
  -w, --warnings             将警告记录为信息级别
      --checkpoint-every N   每N字节创建压缩检查点
  -f, --field NUM            为指定字段创建索引(从1开始计数)
  -d, --delimiter DELIM      使用DELIM作为字段分隔符
      --tab-delimiter        使用制表符作为字段分隔符
```

### zq - 在已索引的压缩文件中搜索
```
用法: zq [选项] 查询字符串 输入文件 [输入文件...]

选项:
  -v, --verbose              显示更详细信息
      --debug                显示调试信息
      --colour, --color      强制使用彩色输出
  -w, --warnings             将警告记录为信息级别
  -i, --index INDEX          使用指定的索引进行搜索
```

## 开发
### 编译
```
cmake . -DStatic:BOOL=On -DCMAKE_BUILD_TYPE=Release
make
```
