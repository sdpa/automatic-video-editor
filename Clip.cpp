//extern "C" {
//#include <libavformat/avformat.h>
//#include <libavcodec/avcodec.h>
//#include <libavutil/dict.h>
//#include <libavdevice/avdevice.h>
//#include <libavutil/log.h>
//#include <libavutil/error.h>
//}
//
//class Clip {
//private:
//	int startPacketIdx;
//	int endPacketIdx;
//	AVPacket* startPacket;
//	AVPacket* endPacket;
//public: 
//	//Constructir
//	Clip* nextClip;
//
//	Clip(int start, int end, AVPacket* s, AVPacket* e) {
//		startPacketIdx = start;
//		endPacketIdx = end;
//		nextClip = NULL;
//		startPacket = s;
//		endPacket = e;
//	}	
//
//	//Get number of packets in clip
//	int getClipLength() const {
//		return endPacketIdx - startPacketIdx;
//	}
//
//	//Returns the first packet in the Clip.
//	AVPacket getClipStart() {
//		return *startPacket;
//	}
//
//	int getClipStartIdx() {
//		return startPacketIdx;
//	}
//
//	//Returns the last packet in the clip.
//	AVPacket getClipEnd() {
//		return *endPacket;
//	}
//
//};