/*
 * Copyright (c) 2018 Vrije Universiteit Amsterdam
 *
 * This program is licensed under the GPL2+.
 */

#ifndef RAMSES_TRANSLATE_HEURISTIC_H
#define RAMSES_TRANSLATE_HEURISTIC_H 1

#include <ramses/translate.h>

void ramses_translate_heuristic(struct Translation *t,
                                int cont_bits, physaddr_t baseaddr);

#endif /* translate_heuristic.h */
