主治：名字服务、负载均衡、故障转移、过载保护

运行：
    wxlistener ip port path/to/l5worker [shmmemorysize]
    如：./wxlistener 0.0.0.0 9527 ./l5worker 0

    ./l5worker -g sid                 查看sid的全部路由配置
    ./l5worker -d sid ip port         删除sid的一条路由配置
    ./l5worker -a sid ip port weight  给sid添加一条路由配置

    客户端获取一条记录：
        curl -H"User-Agent:curl" 127.0.0.1:9527/get/{sid}
        系统自带的curl通常默认会携带一串很长的User-Agent过多的浪费接收缓冲区，所以这里需要覆盖默认值

    清除共享内存：
        ipcrm -M 0x00002537

    批量添加：
        cat sid-ip-port-weight.conf|xargs -n4 ./l5worker -a

类型：
    sid    uint16_t    [0,65535]
    port   uint16_t    [0,65535]
    weight uint8_t     [0,255]

依赖：
    libwxworker git@github.com:renwuxun/wxworker.git


性能：
    在我笔记本(cpu:i7-2620m, mem:12G, disk:ssd 840pro)的虚拟机(cpu:双核四线程, mem:4.5G)里测试：
    l5worker:
        wrk -t4 -c400 http://127.0.0.1:9527/get/123
        Running 10s test @ http://127.0.0.1:9527/get/123
          4 threads and 400 connections
          Thread Stats   Avg      Stdev     Max   +/- Stdev
            Latency     4.28ms    3.18ms  37.46ms   68.54%
            Req/Sec    21.90k     1.35k   35.39k    85.39%
          872338 requests in 10.08s, 64.06MB read
        Requests/sec:  86525.12
        Transfer/sec:  6.35MB
    nginx(静态文件helloword!!!):
        wrk -t4 -c400 http://localhost/index.html
        Running 10s test @ http://localhost/index.html
          4 threads and 400 connections
          Thread Stats   Avg      Stdev     Max   +/- Stdev
            Latency    58.75ms  124.96ms   1.92s    87.39%
            Req/Sec     6.89k     2.03k   13.74k    62.69%
          276147 requests in 10.10s, 70.83MB read
        Requests/sec:  27345.02
        Transfer/sec:  7.01MB

    如你所见
        l5worker: 86525.12 qps
        nginx:    27345.02 qps