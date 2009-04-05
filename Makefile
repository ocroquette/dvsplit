static:
	gcc -g -o dvsplit dvsplit.c /opt/local/lib/libdv.a -I/opt/local/include
	strip dvsplit

dynamic:
	gcc -g -o dvsplit dvsplit.c -I/opt/local/include -L/opt/local/lib -ldv
