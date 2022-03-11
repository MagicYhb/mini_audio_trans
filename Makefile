all:
	gcc audio_trans.c test.c -I./include -I./ -L./lib -lopus -o audio_trans

clean:
	rm -rf audio_trans out.opus out.pcm out.g711a
