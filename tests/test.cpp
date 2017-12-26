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
	cout << "ap::adjust_reader" << endl;

	typedef audio_processor ap;
	bool fail = false;

	auto test = [&](int r, int w, int step_size, int tbl) {
		int delta = ap::adjust_reader(r, w, step_size, tbl);
		r = ap::move_index(r, delta, tbl);
		int df = ap::dist_forward(w, r, tbl);
		int db = ap::dist_backward(w, r, tbl);
		int closest_dist = min(df, db);

		if (std::abs(closest_dist - tbl / 2) >= step_size) {
			fail = true;
			cout << "Fail" << endl;
			cout << "\t" << "closest_dist: " << closest_dist << endl;
			cout << "\t" << "       tbl/2: " << tbl / 2 << endl;
			cout << "\t" << "|dist-tbl/2|: " << std::abs(closest_dist - tbl / 2) << endl;
			cout << "\t" << "   step_size: " << step_size << endl;
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

void utest_advance_index() {
	// Test that the advance_index function moves the supplied index forwared  given
	// the input frequency and the reader/writer positions
	// Test that the reader is placed such that reading VL samples will not read past the writer
	cout << "ap::advance_index" << endl;

	typedef audio_processor ap;

	int tbl = 512*16;
	int w = 0;
	int r = 0;

	// A 93.75hz wave, since SR == 48000 and ABL = 512
	// Each pcm_getter could would return 1 cycle of the wave
	float freq = SR / float(ABL);
	int r_new = ap::advance_index(w, r, freq, tbl);

	// Check that r_new moved according to wave_len
	int wave_len = ABL; // == SR / freq;
	// Check that dist(r, w) is great enough
	int d = min(ap::dist_backward(r_new, r, tbl), ap::dist_forward(r_new, r, tbl));
	if (d % wave_len != 0) {
		cout << "Fail: not moved according to wave_len" << endl;
		return;
	}
	d = ap::dist_forward(r_new, w, tbl);
	if (d < VL)
		cout << "Fail: Reader will read discontinuity" << endl;
	else
		cout << "Pass" << endl;
}

void utest_get_harmonic_less_than() {
	// if get_harmonic_less_than does not return its input multiplied by some non-positive integer power of two,
	// then fail
	// if get_harmonic_less_than does not return a numeber less than or equal to its second argument,
	// then fail
	cout << "ap::get_harmonic_less_than" << endl;

	typedef audio_processor ap;
	float new_freq, freq, power;

	freq = 61.f;
	new_freq = ap::get_harmonic_less_than(freq, 121.f);
	power = std::log2(new_freq / freq);

	if (std::fabs(power - std::floor(power)) > 0.000001) {
		cout << "Fail" << endl;
		return;
	}
	if (power > 0.000001f) {
		cout << "Fail" << endl;
		return;
	}
	if (freq > 121.f) {
		cout << "Fail" << endl;
		return;
	}
	cout << "Pass" << endl;
}

int main() {
	utest_adjust_reader();
	cout << endl;
	utest_advance_index();
	cout << endl;
	utest_get_harmonic_less_than();
	return 0;
}
