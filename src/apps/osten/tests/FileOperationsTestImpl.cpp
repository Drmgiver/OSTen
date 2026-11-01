/* SPDX-License-Identifier: MIT */
// Compile a separate test translation unit without racing Finder's
// FileOperations.o Jam target in parallel OSTen image builds.
#include "../finder/FileOperations.cpp"
