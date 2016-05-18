主治：名字服务、负载均衡、故障转移、过载保护

运行：
	wxlistener ip port path/to/l5worker [shmmemorysize]
	如：./wxlistener 0.0.0.0 9527 ./l5worker 0

	./l5worker -g sid                 查看sid的全部路由配置
	./l5worker -d sid ip port         删除sid的一条路由配置
	./l5worker -a sid ip port weight  给sid添加一条路由配置

依赖：
	libwxworker git@github.com:renwuxun/wxworker.git