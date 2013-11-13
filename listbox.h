/*
 * Generic UI listbox widget internals
 */

#ifndef LISTBOX_H
#define LISTBOX_H

/* Managed context of a scrolling window, of a number of fixed-height
 * lines, backed by a list of a known number of entries */

struct listbox {
    int entries, lines, offset, selected;
};

void listbox_init(struct listbox *s);
void listbox_set_lines(struct listbox *s, unsigned int lines);
void listbox_set_entries(struct listbox *s, unsigned int entries);

/* Scroll functions */

void listbox_up(struct listbox *s, unsigned int n);
void listbox_down(struct listbox *s, unsigned int n);
void listbox_first(struct listbox *s);
void listbox_last(struct listbox *s);
void listbox_to(struct listbox *s, unsigned int n);

int listbox_current(const struct listbox *s);
int listbox_map(const struct listbox *s, int row);

#endif
