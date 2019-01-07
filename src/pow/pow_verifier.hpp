#pragma once

#ifndef _pow_verifier_hpp
#define _pow_verifier_hpp

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "siphash.hpp"
#include "fxp.hpp"
#include "flt.hpp"
#include "galaxy.hpp"
#include "camera.hpp"

namespace prologcoin { namespace pow {

//
// pow difficulty is a value beteen 0..1 (we use fixed-point to represent it)
// It's inverse (1-difficulty) * (2^256)-1 determines the target value
// (hashes must be below target value.)
class pow_difficulty {
public:
    inline pow_difficulty(const flt1648 &val) : difficulty_(val) { 
    }

    // Increase (or decrease) difficulty with a scaling factor 'val'
    inline pow_difficulty scale(const flt1648 &val) const {
	return difficulty_ * val;
    }

    inline void get_target(uint8_t target[32]) const {
	static const flt1648 ONE(1);
	if (difficulty_ == ONE) {
	    memset(target, 0xff, 32);
	    return;
	}
	// Copy inverse difficulty to MSB
	flt1648 normalized_target = difficulty_.reciprocal();
	normalized_target.to_integer(&target[0], 32);
    }

private:
    flt1648 difficulty_;
};

// The PoW proof consists of 32 rows, where each row consists of
// 1 + 7 (i.e. 8) 32-bit integers. The last 7 integers are the id numbers of
// the stars. The first integer is the nonce used for that proof number
// (which is determined by the row number.)
//
class pow_proof {
public:
    static const size_t NUM_ROWS = 32;
    static const size_t ROW_SIZE = 8;
    static const size_t TOTAL_SIZE = ROW_SIZE*NUM_ROWS;
    static const size_t TOTAL_SIZE_BYTES = TOTAL_SIZE*sizeof(uint32_t);

    inline pow_proof() {
	memset(&data_[0], 0, TOTAL_SIZE_BYTES);
    }

    inline pow_proof(uint32_t proof[TOTAL_SIZE]) {
	memcpy(&data_[0], &proof[0], TOTAL_SIZE_BYTES);
    }

    inline void write(uint8_t *bytes) const {
	size_t i = 0;
	for (size_t row = 0; row < NUM_ROWS; row++) {
	    for (size_t j = 0; j < ROW_SIZE; j++) {
		uint32_t word = data_[row*ROW_SIZE+j];
		bytes[i++] = static_cast<uint8_t>((word >> 24) & 0xff);
		bytes[i++] = static_cast<uint8_t>((word >> 16) & 0xff);
		bytes[i++] = static_cast<uint8_t>((word >> 8) & 0xff);
		bytes[i++] = static_cast<uint8_t>((word >> 0) & 0xff);
	    }
	}
    }

    inline size_t num_rows() const { return NUM_ROWS; }

    inline void set_row(size_t row_number, uint32_t row[ROW_SIZE]) {
	memcpy(&data_[row_number*ROW_SIZE], &row[0], ROW_SIZE*sizeof(uint32_t));
    }

    inline void get_row(size_t row_number, uint32_t row[ROW_SIZE]) const {
	memcpy(&row[0], &data_[row_number*ROW_SIZE], ROW_SIZE*sizeof(uint32_t));
    }

    inline size_t mean_nonce() const {
	size_t sum = 0;
	for (size_t row = 0; row < NUM_ROWS; row++) {
	    sum += data_[row*ROW_SIZE+7];
	}
	return sum / NUM_ROWS;
    }

    inline size_t geometric_mean_nonce() const {
	double sumlog = 0;
	for (size_t row = 0; row < NUM_ROWS; row++) {
	    sumlog += log(data_[row*ROW_SIZE+7]);
	}
	return static_cast<size_t>(std::exp(sumlog/NUM_ROWS));
    }
    
private:
    uint32_t data_[ROW_SIZE*NUM_ROWS];
};

class pow_verifier {
public:
    static const int32_t TOLERANCE = 100;

    inline pow_verifier(const siphash_keys &key, size_t super_difficulty) 
        : key_(key), super_difficulty_(super_difficulty) {
	galaxy_ = nullptr;
	camera_ = nullptr;
    }

    inline ~pow_verifier() { cleanup(); }

    bool project_star(size_t id, projected_star &star);
    void setup(size_t proof_number, size_t nonce);
    void cleanup();

private:
    template<size_t N> void setup_t(size_t proof_number, size_t nonce) {
	if (galaxy_ == nullptr) {
	    auto *gal = new galaxy<N,fxp1648>(key_);
	    auto *cam = new camera<N,fxp1648>(*gal,0);
	    galaxy_ = gal;
	    camera_ = cam;
	}
	auto *cam = reinterpret_cast<camera<N,fxp1648> *>(camera_);
	cam->set_target(proof_number, nonce);
    }

    template<size_t N> bool project_star_t(size_t id, projected_star &star) {
	auto *cam = reinterpret_cast<camera<N,fxp1648> *>(camera_);
	return cam->project_star(id, star);
    }

    template<size_t N> void cleanup_t() {
	auto *gal = reinterpret_cast<galaxy<N,fxp1648> *>(galaxy_);
	auto *cam = reinterpret_cast<camera<N,fxp1648> *>(camera_);
        delete gal;
        delete cam;
    }

    const siphash_keys &key_;
    size_t super_difficulty_;
    void *galaxy_;
    void *camera_;
};

bool verify_pow(const siphash_keys &key, size_t super_difficulty, const pow_proof &proof);

bool verify_dipper(const siphash_keys &key, size_t super_difficulty, size_t proof_number, size_t nonce, const uint32_t star_ids[7]);

}}

#endif
