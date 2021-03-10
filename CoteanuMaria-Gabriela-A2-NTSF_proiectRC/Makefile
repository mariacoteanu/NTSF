server:
	clear
	gcc selSNTSF.c -o selSNTSF.bin -l sqlite3 
	./selSNTSF.bin
client:
	clear
	gcc selCNTSF.c -w -o selCNTSF.bin -l sqlite3
	./selCNTSF.bin 127.0.0.1 2728

clean:
	rm -f selCNTSF.bin selSNTSF.bin username.db
	clear
gdb:
	clear
	gcc selSNTSF.c -o selSNTSF.bin -l sqlite3  -O0 -ggdb
	gdb --tui ./selSNTSF.bin
