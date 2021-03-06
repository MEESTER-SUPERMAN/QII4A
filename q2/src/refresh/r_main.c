/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * Refresher setup and main part of the frame generation
 *
 * =======================================================================
 */

#include "header/local.h"

#define NUM_BEAM_SEGS 6

void R_Clear(void);
void R_BeginRegistration(char *map);
struct model_s *R_RegisterModel(char *name);
struct image_s *R_RegisterSkin(char *name);
void R_SetSky(char *name, float rotate, vec3_t axis);
void R_EndRegistration(void);
void R_RenderFrame(refdef_t *fd);
struct image_s *Draw_FindPic(char *name);

void Draw_Pic(int x, int y, char *name);
void Draw_Char(int x, int y, int c);
void Draw_TileClear(int x, int y, int w, int h, char *name);
void Draw_Fill(int x, int y, int w, int h, int c);
void Draw_FadeScreen(void);

viddef_t vid;

refimport_t ri;

int QGL_TEXTURE0, QGL_TEXTURE1;

model_t *r_worldmodel;

float gldepthmin, gldepthmax;

glconfig_t gl_config;
glstate_t gl_state;

image_t *r_notexture; /* use for bad textures */
image_t *r_particletexture; /* little dot for particles */

entity_t *currententity;
model_t *currentmodel;

cplane_t frustum[4];

int r_visframecount; /* bumped when going to a new PVS */
int r_framecount; /* used for dlight push checking */

int c_brush_polys, c_alias_polys;

float v_blend[4]; /* final blending color */

void R_Strings(void);

/* view origin */
vec3_t vup;
vec3_t vpn;
vec3_t vright;
vec3_t r_origin;

float r_world_matrix[16];
float r_base_world_matrix[16];

/* screen size info */
refdef_t r_newrefdef;

int r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;
extern qboolean have_stencil;
unsigned r_rawpalette[256];

cvar_t *gl_norefresh;
cvar_t *gl_drawentities;
cvar_t *gl_drawworld;
cvar_t *gl_speeds;
cvar_t *gl_fullbright;
cvar_t *gl_novis;
cvar_t *gl_nocull;
cvar_t *gl_lerpmodels;
cvar_t *gl_lefthand;
cvar_t *gl_farsee;

cvar_t *gl_lightlevel;
cvar_t *gl_overbrightbits;

cvar_t *gl_nosubimage;
cvar_t *gl_allow_software;

cvar_t *gl_vertex_arrays;

cvar_t *gl_particle_min_size;
cvar_t *gl_particle_max_size;
cvar_t *gl_particle_size;
cvar_t *gl_particle_att_a;
cvar_t *gl_particle_att_b;
cvar_t *gl_particle_att_c;

cvar_t *gl_ext_swapinterval;
cvar_t *gl_ext_palettedtexture;
cvar_t *gl_ext_multitexture;
cvar_t *gl_ext_pointparameters;
cvar_t *gl_ext_compiled_vertex_array;
cvar_t *gl_ext_mtexcombine;

cvar_t *gl_log;
cvar_t *gl_bitdepth;
cvar_t *gl_drawbuffer;
cvar_t *gl_driver;
cvar_t *gl_lightmap;
cvar_t *gl_shadows;
cvar_t *gl_stencilshadow;
cvar_t *gl_mode;

cvar_t *gl_customwidth;
cvar_t *gl_customheight;

#ifdef RETEXTURE
cvar_t *gl_retexturing;
#endif

cvar_t *gl_dynamic;
cvar_t *gl_modulate;
cvar_t *gl_nobind;
cvar_t *gl_round_down;
cvar_t *gl_picmip;
cvar_t *gl_skymip;
cvar_t *gl_showtris;
cvar_t *gl_ztrick;
cvar_t *gl_finish;
cvar_t *gl_clear;
cvar_t *gl_cull;
cvar_t *gl_polyblend;
cvar_t *gl_flashblend;
cvar_t *gl_playermip;
cvar_t *gl_saturatelighting;
cvar_t *gl_swapinterval;
cvar_t *gl_texturemode;
cvar_t *gl_texturealphamode;
cvar_t *gl_texturesolidmode;
cvar_t *gl_anisotropic;
cvar_t *gl_anisotropic_avail;
cvar_t *gl_lockpvs;

cvar_t *vid_fullscreen;
cvar_t *vid_gamma;
cvar_t *vid_ref;

/*
 * Returns true if the box is completely outside the frustom
 */

int androw;
int androh;

void AndroidSetResolution(int aw,int ah)
{
androw=aw;
androh=ah;
}

qboolean
R_CullBox(vec3_t mins, vec3_t maxs)
{
	int i;

	if (gl_nocull->value)
	{
		return false;
	}

	for (i = 0; i < 4; i++)
	{
		if (BOX_ON_PLANE_SIDE(mins, maxs, &frustum[i]) == 2)
		{
			return true;
		}
	}

	return false;
}

void
R_RotateForEntity(entity_t *e)
{
	qglTranslatef(e->origin[0], e->origin[1], e->origin[2]);

	qglRotatef(e->angles[1], 0, 0, 1);
	qglRotatef(-e->angles[0], 0, 1, 0);
	qglRotatef(-e->angles[2], 1, 0, 0);
}

void
R_DrawSpriteModel(entity_t *e)
{
	float alpha = 1.0F;
	vec3_t point;
	dsprframe_t *frame;
	float *up, *right;
	dsprite_t *psprite;

	/* don't even bother culling, because it's just
	   a single polygon without a surface cache */
	psprite = (dsprite_t *)currentmodel->extradata;

	e->frame %= psprite->numframes;
	frame = &psprite->frames[e->frame];

	/* normal sprite */
	up = vup;
	right = vright;

	if (e->flags & RF_TRANSLUCENT)
	{
		alpha = e->alpha;
	}

	if (alpha != 1.0F)
	{
		qglEnable(GL_BLEND);
	}

	qglColor4f(1, 1, 1, alpha);

	R_Bind(currentmodel->skins[e->frame]->texnum);

	R_TexEnv(GL_MODULATE);

	if (alpha == 1.0)
	{
		qglEnable(GL_ALPHA_TEST);
	}
	else
	{
		qglDisable(GL_ALPHA_TEST);
	}

	float texcoords[4][2];
        float verts[4][3];
	
        texcoords[0][0] = 0;      texcoords[0][1] = 1;
        texcoords[1][0] = 0;      texcoords[1][1] = 0;
        texcoords[2][0] = 1;      texcoords[2][1] = 0;
        texcoords[3][0] = 1;      texcoords[3][1] = 1;
	
	VectorMA(e->origin, -frame->origin_y, up, point);
	VectorMA(point, -frame->origin_x, right, point);
	verts[0][0]=point[0];verts[0][1]=point[1];verts[0][2]=point[2];

	VectorMA(e->origin, frame->height - frame->origin_y, up, point);
	VectorMA(point, -frame->origin_x, right, point);
	verts[1][0]=point[0];verts[1][1]=point[1];verts[1][2]=point[2];

	VectorMA(e->origin, frame->height - frame->origin_y, up, point);
	VectorMA(point, frame->width - frame->origin_x, right, point);
	verts[2][0]=point[0];verts[2][1]=point[1];verts[2][2]=point[2];

	VectorMA(e->origin, -frame->origin_y, up, point);
	VectorMA(point, frame->width - frame->origin_x, right, point);
	verts[3][0]=point[0];verts[3][1]=point[1];verts[3][2]=point[2];

        qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
        qglTexCoordPointer( 2, GL_FLOAT, 0, texcoords );
        qglVertexPointer  ( 3, GL_FLOAT, 0, verts );
        qglDrawArrays( GL_TRIANGLE_FAN, 0, 4);
        qglDisableClientState( GL_TEXTURE_COORD_ARRAY );

	qglDisable(GL_ALPHA_TEST);
	R_TexEnv(GL_REPLACE);

	if (alpha != 1.0F)
	{
		qglDisable(GL_BLEND);
	}

	qglColor4f(1, 1, 1, 1);
}

void
R_DrawNullModel(void)
{
	vec3_t shadelight;
	int i;

	if (currententity->flags & RF_FULLBRIGHT)
	{
		shadelight[0] = shadelight[1] = shadelight[2] = 1.0F;
	}
	else
	{
		R_LightPoint(currententity->origin, shadelight);
	}

	qglPushMatrix();
	R_RotateForEntity(currententity);

	qglDisable(GL_TEXTURE_2D);
	qglColor3f(shadelight[0],shadelight[1],shadelight[2]);

	float verts[6][3];	

	verts[0][0]=0;
	verts[0][1]=0;
	verts[0][2]=-16;

	for (i = 0; i <= 4; i++)
	{
		verts[i+1][0]=16 * cos(i * M_PI / 2);
		verts[i+1][1]=16 * sin(i * M_PI / 2);
		verts[i+1][2]=0;
	}

	qglVertexPointer( 3, GL_FLOAT, 0, verts );
        qglDrawArrays( GL_TRIANGLE_FAN, 0, 6);

	verts[0][0]=0;
	verts[0][1]=0;
	verts[0][2]=16;

	for (i = 4; i >= 0; i--)
	{
		verts[4-i+1][0]=16 * cos(i * M_PI / 2);
		verts[4-i+1][1]=16 * sin(i * M_PI / 2);
		verts[4-i+1][2]=0;
	}

	qglVertexPointer( 3, GL_FLOAT, 0, verts );
        qglDrawArrays( GL_TRIANGLE_FAN, 0, 6);

	qglColor3f(1, 1, 1);
	qglPopMatrix();
	qglEnable(GL_TEXTURE_2D);
}

void
R_DrawEntitiesOnList(void)
{
	int i;

	if (!gl_drawentities->value)
	{
		return;
	}

	/* draw non-transparent first */
	for (i = 0; i < r_newrefdef.num_entities; i++)
	{
		currententity = &r_newrefdef.entities[i];

		if (currententity->flags & RF_TRANSLUCENT)
		{
			continue; /* solid */
		}

		if (currententity->flags & RF_BEAM)
		{
			R_DrawBeam(currententity);
		}
		else
		{
			currentmodel = currententity->model;

			if (!currentmodel)
			{
				R_DrawNullModel();
				continue;
			}

			switch (currentmodel->type)
			{
				case mod_alias:
					R_DrawAliasModel(currententity);
					break;
				case mod_brush:
					R_DrawBrushModel(currententity);
					break;
				case mod_sprite:
					R_DrawSpriteModel(currententity);
					break;
				default:
					ri.Sys_Error(ERR_DROP, "Bad modeltype");
					break;
			}
		}
	}

	/* draw transparent entities
	   we could sort these if it ever
	   becomes a problem... */
	qglDepthMask(0);

	for (i = 0; i < r_newrefdef.num_entities; i++)
	{
		currententity = &r_newrefdef.entities[i];

		if (!(currententity->flags & RF_TRANSLUCENT))
		{
			continue; /* solid */
		}

		if (currententity->flags & RF_BEAM)
		{
			R_DrawBeam(currententity);
		}
		else
		{
			currentmodel = currententity->model;

			if (!currentmodel)
			{
				R_DrawNullModel();
				continue;
			}

			switch (currentmodel->type)
			{
				case mod_alias:
					R_DrawAliasModel(currententity);
					break;
				case mod_brush:
					R_DrawBrushModel(currententity);
					break;
				case mod_sprite:
					R_DrawSpriteModel(currententity);
					break;
				default:
					ri.Sys_Error(ERR_DROP, "Bad modeltype");
					break;
			}
		}
	}

	qglDepthMask(1); /* back to writing */
}

void
R_DrawParticles2(int num_particles, const particle_t particles[],
		const unsigned colortable[768])
{
	const particle_t *p;
	int i;
	vec3_t up, right;
	float scale;
	byte color[4];

	R_Bind(r_particletexture->texnum);
	qglDepthMask(GL_FALSE); /* no z buffering */
	qglEnable(GL_BLEND);
	R_TexEnv(GL_MODULATE);

	VectorScale(vup, 1.5, up);
	VectorScale(vright, 1.5, right);

	float colors[num_particles*3][4];
        float verts[num_particles*3][3];
	float texcoords[num_particles*3][2];

	for (p = particles, i = 0; i < num_particles; i++, p++)
	{
		/* hack a scale up to keep particles from disapearing */
		scale = (p->origin[0] - r_origin[0]) * vpn[0] +
				(p->origin[1] - r_origin[1]) * vpn[1] +
				(p->origin[2] - r_origin[2]) * vpn[2];

		if (scale < 20)
		{
			scale = 1;
		}
		else
		{
			scale = 1 + scale * 0.004;
		}

		*(int *)color = colortable[p->color];
		color[3] = p->alpha * 255;

		colors[i*3][0]=color[0]/255.0;
		colors[i*3][1]=color[1]/255.0;
		colors[i*3][2]=color[2]/255.0;
		colors[i*3][3]=color[3]/255.0;
		
		texcoords[i*3][0]=0.0625;
		texcoords[i*3][1]=0.0625;

		verts[i*3][0]=p->origin[0];
		verts[i*3][1]=p->origin[1];
		verts[i*3][2]=p->origin[2];

		colors[i*3+1][0]=color[0]/255.0;
		colors[i*3+1][1]=color[1]/255.0;
		colors[i*3+1][2]=color[2]/255.0;
		colors[i*3+1][3]=color[3]/255.0;
		
		texcoords[i*3+1][0]=1.0625;
		texcoords[i*3+1][1]=0.0625;

		verts[i*3+1][0]=p->origin[0] + up[0] * scale;
		verts[i*3+1][1]=p->origin[1] + up[1] * scale;
		verts[i*3+1][2]=p->origin[2] + up[2] * scale;

		colors[i*3+2][0]=color[0]/255.0;
		colors[i*3+2][1]=color[1]/255.0;
		colors[i*3+2][2]=color[2]/255.0;
		colors[i*3+2][3]=color[3]/255.0;
		
		texcoords[i*3+2][0]=0.0625;
		texcoords[i*3+2][1]=1.0625;

		verts[i*3+2][0]=p->origin[0] + right[0] * scale;
		verts[i*3+2][1]=p->origin[1] + right[1] * scale;
		verts[i*3+2][2]=p->origin[2] + right[2] * scale;
	}

	qglEnableClientState( GL_COLOR_ARRAY );
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
        qglColorPointer( 4, GL_FLOAT, 0, colors );
	qglTexCoordPointer( 2, GL_FLOAT, 0, texcoords );
        qglVertexPointer( 3, GL_FLOAT, 0, verts );
        qglDrawArrays( GL_TRIANGLES, 0, num_particles*3);
        qglDisableClientState( GL_COLOR_ARRAY );
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );

	qglDisable(GL_BLEND);
	qglColor4f(1, 1, 1, 1);
	qglDepthMask(1); /* back to normal Z buffering */
	R_TexEnv(GL_REPLACE);
}

void
R_DrawParticles(void)
{
	#if 0
	if (gl_ext_pointparameters->value && qglPointParameterfEXT)
	{
		int i;
		unsigned char color[4];
		const particle_t *p;

		qglDepthMask(GL_FALSE);
		qglEnable(GL_BLEND);
		qglDisable(GL_TEXTURE_2D);

		qglPointSize(LittleFloat(gl_particle_size->value));

		qglBegin(GL_POINTS);

		for (i = 0, p = r_newrefdef.particles;
			 i < r_newrefdef.num_particles;
			 i++, p++)
		{
			*(int *)color = d_8to24table[p->color & 0xFF];
			color[3] = p->alpha * 255;
			qglColor4ubv(color);
			qglVertex3fv(p->origin);
		}

		qglEnd();

		qglDisable(GL_BLEND);
		qglColor4f(1.0F, 1.0F, 1.0F, 1.0F);
		qglDepthMask(GL_TRUE);
		qglEnable(GL_TEXTURE_2D);
	}
	else
	#endif
	{
		R_DrawParticles2(r_newrefdef.num_particles,
				r_newrefdef.particles, d_8to24table);
	}
}

void
R_PolyBlend(void)
{
	if (!gl_polyblend->value)
	{
		return;
	}

	if (!v_blend[3])
	{
		return;
	}

	qglDisable(GL_ALPHA_TEST);
	qglEnable(GL_BLEND);
	qglDisable(GL_DEPTH_TEST);
	qglDisable(GL_TEXTURE_2D);

	qglLoadIdentity();

	qglRotatef(-90, 1, 0, 0); /* put Z going up */
	qglRotatef(90, 0, 0, 1); /* put Z going up */

	qglColor4f(v_blend[0],v_blend[1],v_blend[2],v_blend[3]);

        float verts[4][3];

	verts[0][0] = 10;verts[0][1] = 100;verts[0][2] = 100;
        verts[1][0] = 10;verts[1][1] = -100;verts[1][2] = 100;
	verts[2][0] = 10;verts[2][1] = -100;verts[2][2] = -100;
	verts[3][0] = 10;verts[3][1] = 100;verts[3][2] = -100;
	
        qglVertexPointer( 3, GL_FLOAT, 0, verts );
        qglDrawArrays( GL_TRIANGLE_FAN, 0, 4);
        
	qglDisable(GL_BLEND);
	qglEnable(GL_TEXTURE_2D);
	qglEnable(GL_ALPHA_TEST);

	qglColor4f(1, 1, 1, 1);
}

int
R_SignbitsForPlane(cplane_t *out)
{
	int bits, j;

	/* for fast box on planeside test */
	bits = 0;

	for (j = 0; j < 3; j++)
	{
		if (out->normal[j] < 0)
		{
			bits |= 1 << j;
		}
	}

	return bits;
}

void
R_SetFrustum(void)
{
	int i;

	/* rotate VPN right by FOV_X/2 degrees */
	RotatePointAroundVector(frustum[0].normal, vup, vpn,
			-(90 - r_newrefdef.fov_x / 2));
	/* rotate VPN left by FOV_X/2 degrees */
	RotatePointAroundVector(frustum[1].normal,
			vup, vpn, 90 - r_newrefdef.fov_x / 2);
	/* rotate VPN up by FOV_X/2 degrees */
	RotatePointAroundVector(frustum[2].normal,
			vright, vpn, 90 - r_newrefdef.fov_y / 2);
	/* rotate VPN down by FOV_X/2 degrees */
	RotatePointAroundVector(frustum[3].normal, vright, vpn,
			-(90 - r_newrefdef.fov_y / 2));

	for (i = 0; i < 4; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct(r_origin, frustum[i].normal);
		frustum[i].signbits = R_SignbitsForPlane(&frustum[i]);
	}
}

void
R_SetupFrame(void)
{
	int i;
	mleaf_t *leaf;

	r_framecount++;

	/* build the transformation matrix for the given view angles */
	VectorCopy(r_newrefdef.vieworg, r_origin);

	AngleVectors(r_newrefdef.viewangles, vpn, vright, vup);

	/* current viewcluster */
	if (!(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
	{
		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;
		leaf = Mod_PointInLeaf(r_origin, r_worldmodel);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		/* check above and below so crossing solid water doesn't draw wrong */
		if (!leaf->contents)
		{   
			/* look down a bit */
			vec3_t temp;

			VectorCopy(r_origin, temp);
			temp[2] -= 16;
			leaf = Mod_PointInLeaf(temp, r_worldmodel);

			if (!(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2))
			{
				r_viewcluster2 = leaf->cluster;
			}
		}
		else
		{
			/* look up a bit */
			vec3_t temp;

			VectorCopy(r_origin, temp);
			temp[2] += 16;
			leaf = Mod_PointInLeaf(temp, r_worldmodel);

			if (!(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2))
			{
				r_viewcluster2 = leaf->cluster;
			}
		}
	}

	for (i = 0; i < 4; i++)
	{
		v_blend[i] = r_newrefdef.blend[i];
	}

	c_brush_polys = 0;
	c_alias_polys = 0;

	/* clear out the portion of the screen that the NOWORLDMODEL defines */
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
	{
		qglEnable(GL_SCISSOR_TEST);
		qglClearColor(0.3, 0.3, 0.3, 1);
		qglScissor(r_newrefdef.x,
				vid.height - r_newrefdef.height - r_newrefdef.y,
				r_newrefdef.width, r_newrefdef.height);
		qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		qglClearColor(1, 0, 0.5, 0.5);
		qglDisable(GL_SCISSOR_TEST);
	}
}

void
R_MYgluPerspective(double fovy, double aspect,
		double zNear, double zFar)
{
	double xmin, xmax, ymin, ymax;

	ymax = zNear * tan(fovy * M_PI / 360.0);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	xmin += -(2 * gl_state.camera_separation) / zNear;
	xmax += -(2 * gl_state.camera_separation) / zNear;

	qglFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
}

void
R_SetupGL(void)
{
	float screenaspect;
	int x, x2, y2, y, w, h;

	/* set up viewport */
	x = floor(r_newrefdef.x * vid.width / vid.width);
	x2 = ceil((r_newrefdef.x + r_newrefdef.width) * vid.width / vid.width);
	y = floor(vid.height - r_newrefdef.y * vid.height / vid.height);
	y2 = ceil(vid.height -
			  (r_newrefdef.y + r_newrefdef.height) * vid.height / vid.height);

	w = x2 - x;
	h = y - y2;

	qglViewport(x, y2, w, h);

	/* set up projection matrix */
	screenaspect = (float)r_newrefdef.width / r_newrefdef.height;
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();

	if (gl_farsee->value == 0)
	{
		R_MYgluPerspective(r_newrefdef.fov_y, screenaspect, 4, 4096);
	}
	else
	{
		R_MYgluPerspective(r_newrefdef.fov_y, screenaspect, 4, 8192);
	}

	qglCullFace(GL_FRONT);

	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();

	qglRotatef(-90, 1, 0, 0); /* put Z going up */
	qglRotatef(90, 0, 0, 1); /* put Z going up */
	qglRotatef(-r_newrefdef.viewangles[2], 1, 0, 0);
	qglRotatef(-r_newrefdef.viewangles[0], 0, 1, 0);
	qglRotatef(-r_newrefdef.viewangles[1], 0, 0, 1);
	qglTranslatef(-r_newrefdef.vieworg[0], -r_newrefdef.vieworg[1],
			-r_newrefdef.vieworg[2]);

	qglGetFloatv(GL_MODELVIEW_MATRIX, r_world_matrix);

	/* set drawing parms */
	if (gl_cull->value)
	{
		qglEnable(GL_CULL_FACE);
	}
	else
	{
		qglDisable(GL_CULL_FACE);
	}

	qglDisable(GL_BLEND);
	qglDisable(GL_ALPHA_TEST);
	qglEnable(GL_DEPTH_TEST);
}

void
R_Clear(void)
{
	if (gl_ztrick->value)
	{
		static int trickframe;

		if (gl_clear->value)
		{
			qglClear(GL_COLOR_BUFFER_BIT);
		}

		trickframe++;

		if (trickframe & 1)
		{
			gldepthmin = 0;
			gldepthmax = 0.49999;
			qglDepthFunc(GL_LEQUAL);
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5;
			qglDepthFunc(GL_GEQUAL);
		}
	}
	else
	{
		if (gl_clear->value)
		{
			qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}
		else
		{
			qglClear(GL_DEPTH_BUFFER_BIT);
		}

		gldepthmin = 0;
		gldepthmax = 1;
		qglDepthFunc(GL_LEQUAL);
	}

	qglDepthRange(gldepthmin, gldepthmax);

	/* stencilbuffer shadows */
	if (gl_shadows->value && have_stencil && gl_stencilshadow->value)
	{
		qglClearStencil(1);
		qglClear(GL_STENCIL_BUFFER_BIT);
	}
}

void
R_Flash(void)
{
	R_PolyBlend();
}

/*
 * r_newrefdef must be set before the first call
 */
void
R_RenderView(refdef_t *fd)
{
	if (gl_norefresh->value)
	{
		return;
	}

	r_newrefdef = *fd;

	if (!r_worldmodel && !(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
	{
		ri.Sys_Error(ERR_DROP, "R_RenderView: NULL worldmodel");
	}

	if (gl_speeds->value)
	{
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	R_PushDlights();

	if (gl_finish->value)
	{
		qglFinish();
	}

	R_SetupFrame();

	R_SetFrustum();

	R_SetupGL();

	R_MarkLeaves(); /* done here so we know if we're in water */

	R_DrawWorld();

	R_DrawEntitiesOnList();

	R_RenderDlights();

	R_DrawParticles();

	R_DrawAlphaSurfaces();

	R_Flash();

	if (gl_speeds->value)
	{
		ri.Con_Printf(PRINT_ALL, "%4i wpoly %4i epoly %i tex %i lmaps\n",
				c_brush_polys, c_alias_polys, c_visible_textures,
				c_visible_lightmaps);
	}
}

void
R_SetGL2D(void)
{
	/* set 2D virtual screen size */
	qglViewport(0, 0, vid.width, vid.height);
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglOrtho(0, vid.width, vid.height, 0, -99999, 99999);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();
	qglDisable(GL_DEPTH_TEST);
	qglDisable(GL_CULL_FACE);
	qglDisable(GL_BLEND);
	qglEnable(GL_ALPHA_TEST);
	qglColor4f(1, 1, 1, 1);
}

void
R_SetLightLevel(void)
{
	vec3_t shadelight;

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
	{
		return;
	}

	/* save off light value for server to look at */
	R_LightPoint(r_newrefdef.vieworg, shadelight);

	/* pick the greatest component, which should be the
	 * same as the mono value returned by software */
	if (shadelight[0] > shadelight[1])
	{
		if (shadelight[0] > shadelight[2])
		{
			gl_lightlevel->value = 150 * shadelight[0];
		}
		else
		{
			gl_lightlevel->value = 150 * shadelight[2];
		}
	}
	else
	{
		if (shadelight[1] > shadelight[2])
		{
			gl_lightlevel->value = 150 * shadelight[1];
		}
		else
		{
			gl_lightlevel->value = 150 * shadelight[2];
		}
	}
}

void
R_RenderFrame(refdef_t *fd)
{
	R_RenderView(fd);
	R_SetLightLevel();
	R_SetGL2D();
}

void
R_Register(void)
{
	gl_lefthand = ri.Cvar_Get("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
	gl_farsee = ri.Cvar_Get("gl_farsee", "0", CVAR_LATCH | CVAR_ARCHIVE);
	gl_norefresh = ri.Cvar_Get("gl_norefresh", "0", 0);
	gl_fullbright = ri.Cvar_Get("gl_fullbright", "0", 0);
	gl_drawentities = ri.Cvar_Get("gl_drawentities", "1", 0);
	gl_drawworld = ri.Cvar_Get("gl_drawworld", "1", 0);
	gl_novis = ri.Cvar_Get("gl_novis", "0", 0);
	gl_nocull = ri.Cvar_Get("gl_nocull", "0", 0);
	gl_lerpmodels = ri.Cvar_Get("gl_lerpmodels", "1", 0);
	gl_speeds = ri.Cvar_Get("gl_speeds", "0", 0);

	gl_lightlevel = ri.Cvar_Get("gl_lightlevel", "0", 0);
	gl_overbrightbits = ri.Cvar_Get("gl_overbrightbits", "2", CVAR_ARCHIVE);

	gl_nosubimage = ri.Cvar_Get("gl_nosubimage", "0", 0);
	gl_allow_software = ri.Cvar_Get("gl_allow_software", "0", 0);

	gl_particle_min_size = ri.Cvar_Get("gl_particle_min_size", "2", CVAR_ARCHIVE);
	gl_particle_max_size = ri.Cvar_Get("gl_particle_max_size", "40", CVAR_ARCHIVE);
	gl_particle_size = ri.Cvar_Get("gl_particle_size", "40", CVAR_ARCHIVE);
	gl_particle_att_a = ri.Cvar_Get("gl_particle_att_a", "0.01", CVAR_ARCHIVE);
	gl_particle_att_b = ri.Cvar_Get("gl_particle_att_b", "0.0", CVAR_ARCHIVE);
	gl_particle_att_c = ri.Cvar_Get("gl_particle_att_c", "0.01", CVAR_ARCHIVE);

	gl_modulate = ri.Cvar_Get("gl_modulate", "1", CVAR_ARCHIVE);
	gl_log = ri.Cvar_Get("gl_log", "0", 0);
	gl_bitdepth = ri.Cvar_Get("gl_bitdepth", "0", 0);
	gl_mode = ri.Cvar_Get("gl_mode", "5", CVAR_ARCHIVE);
	gl_lightmap = ri.Cvar_Get("gl_lightmap", "0", 0);
	gl_shadows = ri.Cvar_Get("gl_shadows", "0", CVAR_ARCHIVE);
	gl_stencilshadow = ri.Cvar_Get("gl_stencilshadow", "0", CVAR_ARCHIVE);
	gl_dynamic = ri.Cvar_Get("gl_dynamic", "1", 0);
	gl_nobind = ri.Cvar_Get("gl_nobind", "0", 0);
	gl_round_down = ri.Cvar_Get("gl_round_down", "1", 0);
	gl_picmip = ri.Cvar_Get("gl_picmip", "0", 0);
	gl_skymip = ri.Cvar_Get("gl_skymip", "0", 0);
	gl_showtris = ri.Cvar_Get("gl_showtris", "0", 0);
	gl_ztrick = ri.Cvar_Get("gl_ztrick", "0", 0);
	gl_finish = ri.Cvar_Get("gl_finish", "0", CVAR_ARCHIVE);
	gl_clear = ri.Cvar_Get("gl_clear", "0", 0);
	gl_cull = ri.Cvar_Get("gl_cull", "1", 0);
	gl_polyblend = ri.Cvar_Get("gl_polyblend", "1", 0);
	gl_flashblend = ri.Cvar_Get("gl_flashblend", "0", 0);
	gl_playermip = ri.Cvar_Get("gl_playermip", "0", 0);
	gl_driver = ri.Cvar_Get("gl_driver", LIBGL, CVAR_ARCHIVE);

	gl_texturemode = ri.Cvar_Get("gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE);
	gl_texturealphamode = ri.Cvar_Get("gl_texturealphamode", "default", CVAR_ARCHIVE);
	gl_texturesolidmode = ri.Cvar_Get("gl_texturesolidmode", "default", CVAR_ARCHIVE);
	gl_anisotropic = ri.Cvar_Get("gl_anisotropic", "0", CVAR_ARCHIVE);
	gl_anisotropic_avail = ri.Cvar_Get("gl_anisotropic_avail", "0", 0);
	gl_lockpvs = ri.Cvar_Get("gl_lockpvs", "0", 0);

	gl_vertex_arrays = ri.Cvar_Get("gl_vertex_arrays", "0", CVAR_ARCHIVE);

	gl_ext_swapinterval = ri.Cvar_Get("gl_ext_swapinterval", "1", CVAR_ARCHIVE);
	gl_ext_palettedtexture = ri.Cvar_Get("gl_ext_palettedtexture", "0", CVAR_ARCHIVE);
	gl_ext_multitexture = ri.Cvar_Get("gl_ext_multitexture", "1", CVAR_ARCHIVE);
	gl_ext_pointparameters = ri.Cvar_Get("gl_ext_pointparameters", "1", CVAR_ARCHIVE);
	gl_ext_compiled_vertex_array = ri.Cvar_Get("gl_ext_compiled_vertex_array", "1", CVAR_ARCHIVE);
	gl_ext_mtexcombine = ri.Cvar_Get("gl_ext_mtexcombine", "1", CVAR_ARCHIVE);

	gl_drawbuffer = ri.Cvar_Get("gl_drawbuffer", "GL_BACK", 0);
	gl_swapinterval = ri.Cvar_Get("gl_swapinterval", "1", CVAR_ARCHIVE);

	gl_saturatelighting = ri.Cvar_Get("gl_saturatelighting", "0", 0);

	vid_fullscreen = ri.Cvar_Get("vid_fullscreen", "0", CVAR_ARCHIVE);
	vid_gamma = ri.Cvar_Get("vid_gamma", "1.0", CVAR_ARCHIVE);
	vid_ref = ri.Cvar_Get("vid_ref", "soft", CVAR_ARCHIVE);

	gl_customwidth = ri.Cvar_Get("gl_customwidth", "1024", CVAR_ARCHIVE);
	gl_customheight = ri.Cvar_Get("gl_customheight", "768", CVAR_ARCHIVE);

#ifdef RETEXTURE
	gl_retexturing = ri.Cvar_Get("gl_retexturing", "1", CVAR_ARCHIVE);
#endif

	ri.Cmd_AddCommand("imagelist", R_ImageList_f);
	ri.Cmd_AddCommand("screenshot", R_ScreenShot);
	ri.Cmd_AddCommand("modellist", Mod_Modellist_f);
	ri.Cmd_AddCommand("gl_strings", R_Strings);
}

qboolean
R_SetMode(void)
{
	rserr_t err;
	qboolean fullscreen;

	fullscreen = vid_fullscreen->value;

	vid_fullscreen->modified = false;
	gl_mode->modified = false;

	/* a bit hackish approach to enable custom resolutions:
	   Glimp_SetMode needs these values set for mode -1 */
	vid.width = androw;//gl_customwidth->value;
	vid.height = androh;//gl_customheight->value;
	gl_mode->value=-1;

	if ((err = GLimp_SetMode(&vid.width, &vid.height, gl_mode->value,
					 fullscreen)) == rserr_ok)
	{
		if (gl_mode->value == -1)
		{
			gl_state.prev_mode = 4; /* safe default for custom mode */
		}
		else
		{
			gl_state.prev_mode = gl_mode->value;
		}
	}
	else
	{
		if (err == rserr_invalid_fullscreen)
		{
			ri.Cvar_SetValue("vid_fullscreen", 0);
			vid_fullscreen->modified = false;
			ri.Con_Printf(PRINT_ALL, "ref_gl::R_SetMode() - fullscreen unavailable in this mode\n");

			if ((err = GLimp_SetMode(&vid.width, &vid.height, gl_mode->value, false)) == rserr_ok)
			{
				return true;
			}
		}
		else if (err == rserr_invalid_mode)
		{
			ri.Cvar_SetValue("gl_mode", gl_state.prev_mode);
			gl_mode->modified = false;
			ri.Con_Printf(PRINT_ALL, "ref_gl::R_SetMode() - invalid mode\n");
		}

		/* try setting it back to something safe */
		if ((err =GLimp_SetMode(&vid.width, &vid.height, gl_state.prev_mode, false)) != rserr_ok)
		{
			ri.Con_Printf(PRINT_ALL, "ref_gl::R_SetMode() - could not revert to safe mode\n");
			return false;
		}
	}

	return true;
}

void (APIENTRY *qglMTexCoord2fSGIS)(GLenum, GLfloat, GLfloat);

int
R_Init(void *hinstance, void *hWnd)
{
	char renderer_buffer[1000];
	char vendor_buffer[1000];
	int err;
	int j;
	extern float r_turbsin[256];

	for (j = 0; j < 256; j++)
	{
		r_turbsin[j] *= 0.5;
	}

	qglEnableClientState(GL_VERTEX_ARRAY);
	qglEnable(GL_TEXTURE0);
	qglEnable(GL_TEXTURE1);

	/* Options */
	ri.Con_Printf(PRINT_ALL, "Refresher build options:\n");
#ifdef RETEXTURE
	ri.Con_Printf(PRINT_ALL, " + Retexturing support\n");
#else
	ri.Con_Printf(PRINT_ALL, " - Retexturing support\n");
#endif
#ifdef X11GAMMA
	ri.Con_Printf(PRINT_ALL, " + Gamma via X11\n");
#else
	ri.Con_Printf(PRINT_ALL, " - Gamma via X11\n");
#endif

	ri.Con_Printf(PRINT_ALL, "Refresh: " REF_VERSION "\n");

	Draw_GetPalette();

	R_Register();

	/* initialize our QGL dynamic bindings */
	/* if (!QGL_Init(gl_driver->string))
	{
		QGL_Shutdown();
		ri.Con_Printf(PRINT_ALL, "ref_gl::R_Init() - could not load \"%s\"\n",
				gl_driver->string);
		return -1;
	}*/

	/* initialize OS-specific parts of OpenGL */
	if (!GLimp_Init())
	{
		//QGL_Shutdown();
		return -1;
	}

	/* set our "safe" modes */
	gl_state.prev_mode = 3;

	/* create the window and set up the context */
	if (!R_SetMode())
	{
		//QGL_Shutdown();
		ri.Con_Printf(PRINT_ALL, "ref_gl::R_Init() - could not R_SetMode()\n");
		return -1;
	}

	ri.Vid_MenuInit();

	/* get our various GL strings */
	ri.Con_Printf(PRINT_ALL, "\nOpenGL setting:\n", gl_config.vendor_string);
	gl_config.vendor_string = (char *)qglGetString(GL_VENDOR);
	ri.Con_Printf(PRINT_ALL, "GL_VENDOR: %s\n", gl_config.vendor_string);
	gl_config.renderer_string = (char *)qglGetString(GL_RENDERER);
	ri.Con_Printf(PRINT_ALL, "GL_RENDERER: %s\n", gl_config.renderer_string);
	gl_config.version_string = (char *)qglGetString(GL_VERSION);
	ri.Con_Printf(PRINT_ALL, "GL_VERSION: %s\n", gl_config.version_string);
	gl_config.extensions_string = (char *)qglGetString(GL_EXTENSIONS);
	ri.Con_Printf(PRINT_ALL, "GL_EXTENSIONS: %s\n", gl_config.extensions_string);

	strncpy(renderer_buffer, gl_config.renderer_string, sizeof(renderer_buffer));
	renderer_buffer[sizeof(renderer_buffer) - 1] = 0;
	strlwr(renderer_buffer);

	strncpy(vendor_buffer, gl_config.vendor_string, sizeof(vendor_buffer));
	vendor_buffer[sizeof(vendor_buffer) - 1] = 0;
	strlwr(vendor_buffer);

	ri.Cvar_Set("scr_drawall", "0");
	gl_config.allow_cds = true;

	ri.Con_Printf(PRINT_ALL, "\n\nProbing for OpenGL extensions:\n");

	/* grab extensions */
	#if 0
	if (strstr(gl_config.extensions_string, "GL_EXT_compiled_vertex_array") ||
		strstr(gl_config.extensions_string, "GL_SGI_compiled_vertex_array"))
	{
		ri.Con_Printf(PRINT_ALL, "...using GL_EXT_compiled_vertex_array\n");
		qglLockArraysEXT = (void *)GetProcAddressGL("glLockArraysEXT");
		qglUnlockArraysEXT = (void *)GetProcAddressGL("glUnlockArraysEXT");
	}
	else
	#endif
	{
		ri.Con_Printf(PRINT_ALL, "...GL_EXT_compiled_vertex_array not found\n");
	}
	#if 0
	if (strstr(gl_config.extensions_string, "GL_EXT_point_parameters"))
	{
		if (gl_ext_pointparameters->value)
		{
			ri.Con_Printf(PRINT_ALL, "...using GL_EXT_point_parameters\n");
			qglPointParameterfEXT = (void (APIENTRY *)(GLenum, GLfloat))
				GetProcAddressGL("glPointParameterfEXT");
			qglPointParameterfvEXT = (void (APIENTRY *)(GLenum, const GLfloat *))
				GetProcAddressGL("glPointParameterfvEXT");
		}
		else
		{
			ri.Con_Printf(PRINT_ALL, "...ignoring GL_EXT_point_parameters\n");
		}
	}
	else
	#endif
	{
		ri.Con_Printf(PRINT_ALL, "...GL_EXT_point_parameters not found\n");
	}
	#if 0
	if (!qglColorTableEXT &&
		strstr(gl_config.extensions_string, "GL_EXT_paletted_texture") &&
		strstr(gl_config.extensions_string, "GL_EXT_shared_texture_palette"))
	{
		if (gl_ext_palettedtexture->value)
		{
			ri.Con_Printf(PRINT_ALL, "...using GL_EXT_shared_texture_palette\n");
			qglColorTableEXT =
				(void (APIENTRY *)(GLenum, GLenum, GLsizei, GLenum, GLenum,
						 const GLvoid *))GetProcAddressGL(
						"glColorTableEXT");
		}
		else
		{
			ri.Con_Printf(PRINT_ALL, "...ignoring GL_EXT_shared_texture_palette\n");
		}
	}
	else
	#endif
	{
		ri.Con_Printf(PRINT_ALL, "...GL_EXT_shared_texture_palette not found\n");
	}

	if (0)//Disabled due to performance reasons
	{
		if (gl_ext_multitexture->value)
		{
			ri.Con_Printf(PRINT_ALL, "...using GL_ARB_multitexture\n");
			qglMTexCoord2fSGIS = 1;
			qglActiveTextureARB = glActiveTexture;
			qglClientActiveTextureARB = glClientActiveTexture;
			QGL_TEXTURE0 = GL_TEXTURE0_ARB;
			QGL_TEXTURE1 = GL_TEXTURE1_ARB;
		}
		else
		{
			ri.Con_Printf(PRINT_ALL, "...ignoring GL_ARB_multitexture\n");
		}
	}
	else
	{
		ri.Con_Printf(PRINT_ALL, "...GL_ARB_multitexture not found\n");
	}
	#if 0
	if (strstr(gl_config.extensions_string, "GL_SGIS_multitexture"))
	{
		if (qglActiveTextureARB)
		{
			ri.Con_Printf(PRINT_ALL, "...GL_SGIS_multitexture deprecated in favor of ARB_multitexture\n");
		}
		else if (gl_ext_multitexture->value)
		{
			ri.Con_Printf(PRINT_ALL, "...using GL_SGIS_multitexture\n");
			qglMTexCoord2fSGIS = (void *)GetProcAddressGL("glMTexCoord2fSGIS");
			qglSelectTextureSGIS = (void *)GetProcAddressGL("glSelectTextureSGIS");
			QGL_TEXTURE0 = GL_TEXTURE0_SGIS;
			QGL_TEXTURE1 = GL_TEXTURE1_SGIS;
		}
		else
		{
			ri.Con_Printf(PRINT_ALL, "...ignoring GL_SGIS_multitexture\n");
		}
	}
	else
	#endif
	{
		ri.Con_Printf(PRINT_ALL, "...GL_SGIS_multitexture not found\n");
	}

	gl_config.anisotropic = false;

	if (strstr(gl_config.extensions_string, "GL_EXT_texture_filter_anisotropic"))
	{
		ri.Con_Printf(PRINT_ALL, "...using GL_EXT_texture_filter_anisotropic\n");
		gl_config.anisotropic = true;
		qglGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_config.max_anisotropy);
		ri.Cvar_SetValue("gl_anisotropic_avail", gl_config.max_anisotropy);
	}
	else
	{
		ri.Con_Printf(PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not found\n");
		gl_config.anisotropic = false;
		gl_config.max_anisotropy = 0.0;
		ri.Cvar_SetValue("gl_anisotropic_avail", 0.0);
	}

	gl_config.mtexcombine = false;

	if (strstr(gl_config.extensions_string, "GL_ARB_texture_env_combine"))
	{
		if (gl_ext_mtexcombine->value)
		{
			ri.Con_Printf(PRINT_ALL, "...using GL_ARB_texture_env_combine\n");
			gl_config.mtexcombine = true;
		}
		else
		{
			ri.Con_Printf(PRINT_ALL, "...ignoring GL_ARB_texture_env_combine\n");
		}
	}
	else
	{
		ri.Con_Printf(PRINT_ALL, "...GL_ARB_texture_env_combine not found\n");
	}

	if (!gl_config.mtexcombine)
	{
		if (strstr(gl_config.extensions_string, "GL_EXT_texture_env_combine"))
		{
			if (gl_ext_mtexcombine->value)
			{
				ri.Con_Printf(PRINT_ALL, "...using GL_EXT_texture_env_combine\n");
				gl_config.mtexcombine = true;
			}
			else
			{
				ri.Con_Printf(PRINT_ALL, "...ignoring GL_EXT_texture_env_combine\n");
			}
		}
		else
		{
			ri.Con_Printf(PRINT_ALL, "...GL_EXT_texture_env_combine not found\n");
		}
	}

	R_SetDefaultState();

	R_InitImages();
	Mod_Init();
	R_InitParticleTexture();
	Draw_InitLocal();

	err = qglGetError();

	if (err != GL_NO_ERROR)
	{
		ri.Con_Printf(PRINT_ALL, "glGetError() = 0x%x\n", err);
	}

	return true;
}

void
R_Shutdown(void)
{
	ri.Cmd_RemoveCommand("modellist");
	ri.Cmd_RemoveCommand("screenshot");
	ri.Cmd_RemoveCommand("imagelist");
	ri.Cmd_RemoveCommand("gl_strings");

	Mod_FreeAll();

	R_ShutdownImages();

	/* shutdown OS specific OpenGL stuff like contexts, etc.  */
	GLimp_Shutdown();

	/* shutdown our QGL subsystem */
	//QGL_Shutdown();
}

extern void UpdateHardwareGamma();

void
R_BeginFrame(float camera_separation)
{
	gl_state.camera_separation = camera_separation;

	/* change modes if necessary */
	if (gl_mode->modified || vid_fullscreen->modified)
	{
		cvar_t *ref;

		ref = ri.Cvar_Get("vid_ref", "gl", 0);
		ref->modified = true;
	}

	if (gl_log->modified)
	{
		//GLimp_EnableLogging(gl_log->value);
		gl_log->modified = false;
	}

	if (gl_log->value)
	{
		//GLimp_LogNewFrame();
	}

	if (vid_gamma->modified)
	{
		vid_gamma->modified = false;

		if (gl_state.hwgamma)
		{
			UpdateHardwareGamma();
		}
	}

	/* go into 2D mode */
	qglViewport(0, 0, vid.width, vid.height);
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglOrtho(0, vid.width, vid.height, 0, -99999, 99999);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();
	qglDisable(GL_DEPTH_TEST);
	qglDisable(GL_CULL_FACE);
	qglDisable(GL_BLEND);
	qglEnable(GL_ALPHA_TEST);
	qglColor4f(1, 1, 1, 1);

	/* draw buffer stuff */
	if (gl_drawbuffer->modified)
	{
		gl_drawbuffer->modified = false;

		if ((gl_state.camera_separation == 0) || !gl_state.stereo_enabled)
		{
			if (Q_stricmp(gl_drawbuffer->string, "GL_FRONT") == 0)
			{
				//qglDrawBuffer(GL_FRONT);
			}
			else
			{
				//qglDrawBuffer(GL_BACK);
			}
		}
	}

	/* texturemode stuff */
	if (gl_texturemode->modified)
	{
		R_TextureMode(gl_texturemode->string);
		gl_texturemode->modified = false;
	}

	if (gl_texturealphamode->modified)
	{
		R_TextureAlphaMode(gl_texturealphamode->string);
		gl_texturealphamode->modified = false;
	}

	if (gl_texturesolidmode->modified)
	{
		R_TextureSolidMode(gl_texturesolidmode->string);
		gl_texturesolidmode->modified = false;
	}

	/* clear screen if desired */
	R_Clear();
}

void
R_SetPalette(const unsigned char *palette)
{
	int i;

	byte *rp = (byte *)r_rawpalette;

	if (palette)
	{
		for (i = 0; i < 256; i++)
		{
			rp[i * 4 + 0] = palette[i * 3 + 0];
			rp[i * 4 + 1] = palette[i * 3 + 1];
			rp[i * 4 + 2] = palette[i * 3 + 2];
			rp[i * 4 + 3] = 0xff;
		}
	}
	else
	{
		for (i = 0; i < 256; i++)
		{
			rp[i * 4 + 0] = LittleLong(d_8to24table[i]) & 0xff;
			rp[i * 4 + 1] = (LittleLong(d_8to24table[i]) >> 8) & 0xff;
			rp[i * 4 + 2] = (LittleLong(d_8to24table[i]) >> 16) & 0xff;
			rp[i * 4 + 3] = 0xff;
		}
	}

	R_SetTexturePalette(r_rawpalette);

	qglClearColor(0, 0, 0, 0);
	qglClear(GL_COLOR_BUFFER_BIT);
	qglClearColor(1, 0, 0.5, 0.5);
}

/* R_DrawBeam */
void
R_DrawBeam(entity_t *e)
{
	int i;
	float r, g, b;

	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if (VectorNormalize(normalized_direction) == 0)
	{
		return;
	}

	PerpendicularVector(perpvec, normalized_direction);
	VectorScale(perpvec, e->frame / 2, perpvec);

	for (i = 0; i < 6; i++)
	{
		RotatePointAroundVector(start_points[i], normalized_direction, perpvec,
				(360.0 / NUM_BEAM_SEGS) * i);
		VectorAdd(start_points[i], origin, start_points[i]);
		VectorAdd(start_points[i], direction, end_points[i]);
	}

	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_BLEND);
	qglDepthMask(GL_FALSE);

	r = (LittleLong(d_8to24table[e->skinnum & 0xFF])) & 0xFF;
	g = (LittleLong(d_8to24table[e->skinnum & 0xFF]) >> 8) & 0xFF;
	b = (LittleLong(d_8to24table[e->skinnum & 0xFF]) >> 16) & 0xFF;

	r *= 1 / 255.0F;
	g *= 1 / 255.0F;
	b *= 1 / 255.0F;

	qglColor4f(r, g, b, e->alpha);

	float verts[NUM_BEAM_SEGS*4][3];

	for (i = 0; i < NUM_BEAM_SEGS; i++)
	{
		verts[i*4][0]=start_points[i][0];
		verts[i*4][1]=start_points[i][1];
		verts[i*4][2]=start_points[i][2];

		verts[i*4+1][0]=end_points[i][0];
		verts[i*4+1][1]=end_points[i][1];
		verts[i*4+1][2]=end_points[i][2];

		verts[i*4+2][0]=start_points[(i + 1) % NUM_BEAM_SEGS][0];
		verts[i*4+2][1]=start_points[(i + 1) % NUM_BEAM_SEGS][1];
		verts[i*4+2][2]=start_points[(i + 1) % NUM_BEAM_SEGS][2];

		verts[i*4+3][0]=end_points[(i + 1) % NUM_BEAM_SEGS][0];
		verts[i*4+3][1]=end_points[(i + 1) % NUM_BEAM_SEGS][1];
		verts[i*4+3][2]=end_points[(i + 1) % NUM_BEAM_SEGS][2];
	}

	qglVertexPointer( 3, GL_FLOAT, 0, verts);
        qglDrawArrays( GL_TRIANGLE_STRIP, 0, NUM_BEAM_SEGS*4);

	qglEnable(GL_TEXTURE_2D);
	qglDisable(GL_BLEND);
	qglDepthMask(GL_TRUE);
}

refexport_t
R_GetRefAPI(refimport_t rimp)
{
	refexport_t re;

	ri = rimp;

	re.api_version = API_VERSION;

	re.BeginRegistration = R_BeginRegistration;
	re.RegisterModel = R_RegisterModel;
	re.RegisterSkin = R_RegisterSkin;
	re.RegisterPic = Draw_FindPic;
	re.SetSky = R_SetSky;
	re.EndRegistration = R_EndRegistration;

	re.RenderFrame = R_RenderFrame;

	re.DrawGetPicSize = Draw_GetPicSize;
	re.DrawPic = Draw_Pic;
	re.DrawStretchPic = Draw_StretchPic;
	re.DrawChar = Draw_Char;
	re.DrawTileClear = Draw_TileClear;
	re.DrawFill = Draw_Fill;
	re.DrawFadeScreen = Draw_FadeScreen;

	re.DrawStretchRaw = Draw_StretchRaw;

	re.Init = R_Init;
	re.Shutdown = R_Shutdown;

	re.CinematicSetPalette = R_SetPalette;
	re.BeginFrame = R_BeginFrame;
	re.EndFrame = GLimp_EndFrame;

	re.AppActivate = NULL;

	Swap_Init();

	return re;
}

/*
 * this is only here so the functions
 * in shared source files can link
 */
void
Sys_Error(char *error, ...)
{
	va_list argptr;
	char text[1024];

	va_start(argptr, error);
	vsprintf(text, error, argptr);
	va_end(argptr);

	ri.Sys_Error(ERR_FATAL, "%s", text);
}

void
Com_Printf(char *fmt, ...)
{
	va_list argptr;
	char text[1024];

	va_start(argptr, fmt);
	vsprintf(text, fmt, argptr);
	va_end(argptr);

	ri.Con_Printf(PRINT_ALL, "%s", text);
}

