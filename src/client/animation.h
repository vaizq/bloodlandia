#ifndef ANIMATION_H
#define ANIMATION_H

#include <vector>
#include <cstdio>
#include "util.h"

class Animation {
public:
	Animation(const char* dirpath, Clock::duration duration) 
	: duration{duration}
	{
		rl::FilePathList files = rl::LoadDirectoryFiles(dirpath);

		if (files.count > files.capacity) {
			throw std::runtime_error(std::format("count {} > capacity {}\n", files.count, files.capacity));
		}
		
		std::vector<std::string> filenames;
		for (int i = 0; i < files.count; ++i) {
			filenames.push_back(files.paths[i]);
		}

		rl::UnloadDirectoryFiles(files);


		std::sort(filenames.begin(), filenames.end());

		for (const auto& filename : filenames) {
			auto texture = rl::LoadTexture(filename.c_str());
			frames.push_back(texture);
		}

		std::printf("INFO\tloaded animation containing %ud frames from %s\n", files.count, dirpath);

		frametime = duration / files.count;
	}

	/*
	~Animation() {
		printf("~Animation() called\n");
		for(auto& frame : frames) {
			rl::UnloadTexture(frame);
		}
		frames.clear();
	}
	*/

	rl::Texture& operator[](size_t idx) {
		return frames.at(idx);
	}

	rl::Texture2D& currentFrame() {
		return frames[currentFrameIdx];
	}

	void reset() {
		currentFrameIdx = 0;
		fromPrev = Clock::duration{0};
	}

	void update(Clock::duration dt) {
		fromPrev += dt;
		if (fromPrev > frametime) {
			if (++currentFrameIdx == frames.size()) {
				currentFrameIdx = 0;
			}

			fromPrev = Clock::duration{0};
		}
	}
private:
	std::vector<rl::Texture2D> frames;
	size_t currentFrameIdx{0};
	Clock::duration duration;
	Clock::duration frametime;
	Clock::duration fromPrev{0};
};


#endif
