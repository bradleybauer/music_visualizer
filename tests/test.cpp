#include "../audio_process.h"

// TODO finish writing these tests.

using std::cout;
using std::endl;

template<typename T>
T min(T a, T b) {
	if (a < b) return a;
	return b;
}

void utest_adjust_reader() {
	// adjust_reader attempts to separate two points r (reader) and w (writer)
	// in a circular buffer by distance tbl/2.f by moving r in steps of size step_size

	// adjust_reader will fail if |w - r| is more than step_size units away from tbl/2

	cout << "audio_processor::adjust_reader" << endl;
	typedef audio_processor ap;
	bool fail = false;

	auto test = [&](int r, int w, int step_size, int tbl) {
		int delta = ap::adjust_reader(r, w, step_size, tbl);
		r = ap::move_index(r, delta, tbl);
		int df = ap::dist_forward(w, r, tbl);
		int db = ap::dist_backward(w, r, tbl);
		int closest_dist = min(df, db);

		cout << "\t" << "closest_dist: " << closest_dist << endl;
		cout << "\t" << "       tbl/2: " << tbl / 2 << endl;
		cout << "\t" << "|dist-tbl/2|: " << std::abs(closest_dist - tbl / 2) << endl;
		cout << "\t" << "   step_size: " << step_size << endl;
		if (std::abs(closest_dist - tbl / 2) >= step_size) {
			fail = true;
		}
	};

	int tbl;
	int step_size, w, r;

	tbl = 512 * 16;
	step_size = 1000;
	//step_size = .75;
	//step_size = 1.;
	w = 0;
	r = tbl;
	test(r, w, step_size, tbl);

	tbl = 52 * 16;
	step_size = 10;
	w = 0;
	r = tbl;
	test(r, w, step_size, tbl);

	if (fail) {
		cout << "Fail" << endl;
	}
	else {
		cout << "Pass" << endl;
	}
}

// TODO: audio buffers do not need to be initialized to test advance_index()
void utest_advance_index() {
	// Test that the advance_index function moves the supplied index forwared properly given
	// the input frequency and the reader/writer positions
	// Test that the error_msg function is called in case the reader is too close to the writer

	cout << "audio_processor::advance_index" << endl;
	typedef audio_processor ap;

	bool did_error = false;
	auto error_msg = [&did_error]() { did_error = true; };

	int tbl = 512*16;
	int w = 0;
	int r = 0;

	// A 93.75hz wave, since SR == 48000 and ABL = 512
	// Each pcm_getter could would return 1 cycle of the wave
	float freq = SR / float(ABL);
	float wave_len = float(ABL); // == SR / freq;
	cout << "freq: " << freq << endl;
	cout << "r : " << r << endl;
	int r_new = ap::advance_index(w, r, freq, tbl);

	// Check that r_new moved according to wave_len
	// Check that dist(r, w) is great enough
	int df = ap::dist_forward(r, w, tbl);
	int db = ap::dist_backward(r, w, tbl);
}

void utest_get_harmonic() {
	// if get_harmonic does not return its input multiplied by some integer power of two,
	// then fail

	typedef audio_processor ap;
	float h_freq, freq, p2;

	freq = 61.f;
	h_freq = ap::get_harmonic(freq);
	p2 = std::log2(h_freq / freq);

	cout << freq << endl;
	cout << h_freq << endl;
	cout << p2 << endl;
}

int main() {
	utest_adjust_reader();
	cout << endl;
	utest_advance_index();
	cout << endl;
	utest_get_harmonic();
	return 0;
}
