/* logo.h - the title: 'TEMPEST' and '2000' as solid letters in space. */
#ifndef T2K_LOGO_H
#define T2K_LOGO_H

/* Draw the title's lettering as it stands `frames` frames in. The view must
 * have begun, with the viewer at the origin. */
void logo_title(unsigned frames);

/* The same for the score page's heading, 'TOP GUNS', at the top. */
void logo_topguns(unsigned frames);

/* And the credits page's, 'TEMPEST DUDES'. */
void logo_dudes(unsigned frames);

/* And the page's where a player gives their initials, 'GREAT SCORE'. */
void logo_greatscore(unsigned frames);

#endif /* T2K_LOGO_H */
