# inotify(sys/inotify.h)（linux 3.10）

linux系统监听目录的所有操作（新建，访问，修改，移动，删除等操作）,适合网盘那种自动更新的应用场景。

| 事件常量         | 含义                              |
|------------------|-----------------------------------|
| IN_ACCESS	   | 文件被访问                        |
| IN_ATTRIB        | 文件元数据改变                    |
| IN_CLOSE_WRITE   | 关闭为了写入而打开的文件          |
| IN_CLOSE_NOWRITE | 关闭只读方式打开的文件            |
| IN_CREATE        | 在监听目录内创建了文件/目录       |
| IN_DELETE        | 在监听目录内删除文件/目录         |
| IN_DELETE_SELF   | 监听目录/文件本身被删除           |
| IN_MODIFY        | 文件被修改                        |
| IN_MOVE_SELF     | 受监控目录/文件本身被移动         |
| IN_MOVED         | 文件被移动                        |
| IN_OPEN          | 文件被打开                        |
| IN_ALL_EVENTS    | 以上所有输出事件的统称            |