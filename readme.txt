主治：名字服务、负载均衡、故障转移、过载保护

运行：
	wxlistener ip port path/to/l5worker [shmmemorysize]
	如：./wxlistener 0.0.0.0 9527 ./l5worker 0

	./l5worker -g sid                 查看sid的全部路由配置
	./l5worker -d sid ip port         删除sid的一条路由配置
	./l5worker -a sid ip port weight  给sid添加一条路由配置

	客户端获取一条记录：
		curl -H"User-Agent:curl" 127.0.0.1:9527/get/{sid}
		因为系统自带的curl通常默认会携带一串长长长长长长长长长长长长长...的User-Agent过多的浪费接收缓冲区，所以这里需要覆盖默认值
说明：
	sid    uint16_t    [0,65535]
	port   uint16_t    [0,65535]
	weight uint8_t     [0,255]
依赖：
	libwxworker git@github.com:renwuxun/wxworker.git