// -*- C++ -*-

/**
 * @file   seedmanager.h
 * @brief  Manages random number seeds with file I/O.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */

#ifndef DH_SEEDMANAGER_H
#define DH_SEEDMANAGER_H

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <atomic>
#include <mutex>

#include "mwc64.h"

class SeedManager {
private:
  // Constants
  static constexpr int BUFFER_SIZE = 64;
  static constexpr int MAX_DIGITS = 20;
  static constexpr int FILE_PERMISSIONS = 0644;
  static constexpr unsigned int DEFAULT_SEED1 = 12345;
  static constexpr unsigned int DEFAULT_SEED2 = 67890;
  static constexpr unsigned int FALLBACK_SEED1 = 1;
  static constexpr unsigned int FALLBACK_SEED2 = 2;
  
  // Read an integer from file
  static uint64_t readIntFromFile(int fd) {
    char buf[BUFFER_SIZE] = {0};
    int pos = 0;
    int bytes_read = 0;
    
    // Read until newline
    while ((bytes_read = read(fd, buf + pos, 1)) > 0) {
      if (buf[pos] == '\n' || buf[pos] == ' ') {
        buf[pos] = '\0';
        break;
      }
      pos++;
      if (pos >= BUFFER_SIZE - 1) break;
    }
    
    // Convert to integer
    uint64_t result = 0;
    if (pos > 0) {
      for (int i = 0; buf[i] != '\0'; i++) {
        if (buf[i] >= '0' && buf[i] <= '9') {
          result = result * 10 + (buf[i] - '0');
        }
      }
    }
    
    return result;
  }
  
  // Write an integer to file with newline
  static void writeIntToFile(int fd, unsigned int value) {
    char buf[BUFFER_SIZE] = {0};
    int pos = 0;
    int digits[MAX_DIGITS] = {0};
    int digit_count = 0;
    
    // Handle zero case
    if (value == 0) {
      buf[pos++] = '0';
    } else {
      // Extract digits in reverse order
      while (value > 0) {
        digits[digit_count++] = value % 10;
        value /= 10;
      }
      
      // Write digits in correct order
      for (int i = digit_count - 1; i >= 0; i--) {
        buf[pos++] = '0' + digits[i];
      }
    }
    
    buf[pos++] = '\n';
    write(fd, buf, pos);
  }

  // Static mutex for thread synchronization
  static std::mutex seedMutex;
  static std::atomic<bool> initialized;
  static unsigned int currentSeed1;
  static unsigned int currentSeed2;

public:
  // Get a pair of seeds, ensuring they're unique for each call
  static void getSeedPair(unsigned int& seed1, unsigned int& seed2) {
    std::lock_guard<std::mutex> lock(seedMutex);
    
    if (!initialized.load(std::memory_order_acquire)) {
      initializeSeeds();
      initialized.store(true, std::memory_order_release);
    }
    
    // Return current seeds
    seed1 = currentSeed1;
    seed2 = currentSeed2;
    
    // Update seeds for next call
    updateSeeds();
  }

private:
  // Initialize seeds from file or generate new ones
  static void initializeSeeds() {
    auto *inputFile = getenv("DIEHARD_RANDOM_SEED_INPUT_FILE");
    if (inputFile) {
      int fd = open(inputFile, O_RDONLY);
      if (fd >= 0) {
        currentSeed1 = readIntFromFile(fd);
        currentSeed2 = readIntFromFile(fd);
        close(fd);
      } else {
        generateNewSeeds();
      }
    } else {
      generateNewSeeds();
    }
    
    // Ensure seeds are never zero
    if (currentSeed1 == 0) currentSeed1 = FALLBACK_SEED1;
    if (currentSeed2 == 0) currentSeed2 = FALLBACK_SEED2;
    
    // Write seeds to output file if requested
    auto *outputFile = getenv("DIEHARD_RANDOM_SEED_OUTPUT_FILE");
    if (outputFile) {
      int fd = open(outputFile, O_WRONLY | O_CREAT | O_APPEND, FILE_PERMISSIONS);
      if (fd >= 0) {
        writeIntToFile(fd, currentSeed1);
        writeIntToFile(fd, currentSeed2);
        close(fd);
      }
    }
  }
  
  // Generate new seeds using platform-specific randomness
  static void generateNewSeeds() {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
      read(fd, &currentSeed1, sizeof(currentSeed1));
      read(fd, &currentSeed2, sizeof(currentSeed2));
      close(fd);
    } else {
      // Fallback if /dev/urandom is not available
      currentSeed1 = DEFAULT_SEED1;
      currentSeed2 = DEFAULT_SEED2;
    }
  }
  
  // Update seeds for next call using MWC algorithm
  static void updateSeeds() {
    // Use MWC64 to generate next seeds
    static MWC64 seedGen(currentSeed1, currentSeed2);
    
    // Get next value and ensure seeds are never zero
    currentSeed1 = seedGen.next();
    if (currentSeed1 == 0) currentSeed1 = FALLBACK_SEED1;
    
    currentSeed2 = seedGen.next();
    if (currentSeed2 == 0) currentSeed2 = FALLBACK_SEED2;
  }
};

// Define static members
std::mutex SeedManager::seedMutex;
std::atomic<bool> SeedManager::initialized(false);
unsigned int SeedManager::currentSeed1 = 0;
unsigned int SeedManager::currentSeed2 = 0;

#endif