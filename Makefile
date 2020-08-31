all: diskinfo disklist diskget diskput

diskinfo: diskinfo.cpp
	g++ -g -Wall -std=c++11 diskinfo.cpp -o diskinfo

disklist: disklist.cpp
	g++ -g -Wall -std=c++11 disklist.cpp -o disklist

diskget: diskget.cpp
	g++ -g -Wall -std=c++11 diskget.cpp -o diskget

diskput: diskput.cpp
	g++ -g -Wall -std=c++11 diskput.cpp -o diskput

clean:
	rm -f diskinfo disklist diskget diskput *.o
