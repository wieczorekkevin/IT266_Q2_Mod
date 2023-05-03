/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include "g_local.h"
#include "m_player.h"

//kmw g_cmds.c Globals
int blasterSkill;
int shotgunSkill;
edict_t* grenadelauncherMine;
int grenadelauncherSkill = 0;
int hyperblasterSkill = 0;
float hyperblasterCooldown;
int railgunSkill = 0;
edict_t* teleporter;
int rocketlauncherSkill = 0;
float rocketlauncherCooldown;

int gameActive = 0;
int waveActive = 0;
int shopActive = 0;
int waveNumber = 1;
int money = 0;
int waveTimer = 0;
int enemyCount = 0;
int waveTimerUpdater = 0.0f;

char *ClientTeam (edict_t *ent)
{
	char		*p;
	static char	value[512];

	value[0] = 0;

	if (!ent->client)
		return value;

	strcpy(value, Info_ValueForKey (ent->client->pers.userinfo, "skin"));
	p = strchr(value, '/');
	if (!p)
		return value;

	if ((int)(dmflags->value) & DF_MODELTEAMS)
	{
		*p = 0;
		return value;
	}

	// if ((int)(dmflags->value) & DF_SKINTEAMS)
	return ++p;
}

qboolean OnSameTeam (edict_t *ent1, edict_t *ent2)
{
	char	ent1Team [512];
	char	ent2Team [512];

	if (!((int)(dmflags->value) & (DF_MODELTEAMS | DF_SKINTEAMS)))
		return false;

	strcpy (ent1Team, ClientTeam (ent1));
	strcpy (ent2Team, ClientTeam (ent2));

	if (strcmp(ent1Team, ent2Team) == 0)
		return true;
	return false;
}


void SelectNextItem (edict_t *ent, int itflags)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;

	cl = ent->client;

	if (cl->chase_target) {
		ChaseNext(ent);
		return;
	}

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (cl->pers.selected_item + i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (!(it->flags & itflags))
			continue;

		cl->pers.selected_item = index;
		return;
	}

	cl->pers.selected_item = -1;
}

void SelectPrevItem (edict_t *ent, int itflags)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;

	cl = ent->client;

	if (cl->chase_target) {
		ChasePrev(ent);
		return;
	}

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (cl->pers.selected_item + MAX_ITEMS - i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (!(it->flags & itflags))
			continue;

		cl->pers.selected_item = index;
		return;
	}

	cl->pers.selected_item = -1;
}

void ValidateSelectedItem (edict_t *ent)
{
	gclient_t	*cl;

	cl = ent->client;

	if (cl->pers.inventory[cl->pers.selected_item])
		return;		// valid

	SelectNextItem (ent, -1);
}

//kmw Specific Ability Functions

static void P_ProjectSource(gclient_t* client, vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result)
{
	vec3_t	_distance;

	VectorCopy(distance, _distance);
	if (client->pers.hand == LEFT_HANDED)
		_distance[1] *= -1;
	else if (client->pers.hand == CENTER_HANDED)
		_distance[1] = 0;
	G_ProjectSource(point, _distance, forward, right, result);
}

//kmw Super Shotgun Code
void Alt_SuperShotgun_BoltTouch(edict_t* self, edict_t* other, cplane_t* plane, csurface_t* surf) {
	int	mod = MOD_HYPERBLASTER;

	if (other == self->owner)
		return;

	if (surf && (surf->flags & SURF_SKY))
	{
		G_FreeEdict(self);
		return;
	}

	if (self->owner->client)
		PlayerNoise(self->owner, self->s.origin, PNOISE_IMPACT);

	if (other->takedamage)
	{
		//gi.bprintf(PRINT_HIGH, "%s old position\n", vtos(self->s.origin));
		VectorCopy(other->s.origin, self->owner->s.origin);
		VectorCopy(other->s.origin, self->owner->s.old_origin);
		//gi.bprintf(PRINT_HIGH, "%s new position\n", vtos(self->s.origin));
		T_Damage(other, self, self->owner, self->velocity, self->s.origin, plane->normal, 10000, 1, DAMAGE_ENERGY, mod);
	}
	else
	{
		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_BLASTER);
		gi.WritePosition(self->s.origin);
		if (!plane)
			gi.WriteDir(vec3_origin);
		else
			gi.WriteDir(plane->normal);
		gi.multicast(self->s.origin, MULTICAST_PVS);
	}

	G_FreeEdict(self);
}

void fire_supershotgun_alt(edict_t* self, vec3_t start, vec3_t dir, int damage, int speed, int effect)
{
	edict_t* bolt;
	trace_t	tr;

	VectorNormalize(dir);

	bolt = G_Spawn();
	bolt->svflags = SVF_DEADMONSTER;

	VectorCopy(start, bolt->s.origin);
	VectorCopy(start, bolt->s.old_origin);
	vectoangles(dir, bolt->s.angles);
	VectorScale(dir, speed, bolt->velocity);
	bolt->movetype = MOVETYPE_FLYMISSILE;
	bolt->clipmask = MASK_SHOT;
	bolt->solid = SOLID_BBOX;
	bolt->s.effects |= effect;
	VectorClear(bolt->mins);
	VectorClear(bolt->maxs);
	bolt->s.modelindex = gi.modelindex("models/objects/laser/tris.md2");
	bolt->s.sound = gi.soundindex("misc/lasfly.wav");
	bolt->owner = self;
	bolt->touch = Alt_SuperShotgun_BoltTouch;
	bolt->nextthink = level.time + 2;
	bolt->think = G_FreeEdict;
	bolt->dmg = damage;
	bolt->classname = "bolt";

	tr = gi.trace(self->s.origin, NULL, NULL, bolt->s.origin, bolt, MASK_SHOT);
	if (tr.fraction < 1.0)
	{
		VectorMA(bolt->s.origin, -10, dir, bolt->s.origin);
		bolt->touch(bolt, tr.ent, NULL, NULL);
	}
}

void Alt_SuperShotgun_Fire(edict_t* ent, vec3_t g_offset, int damage, int effect) {
	vec3_t	forward, right;
	vec3_t	start;
	vec3_t	offset;

	AngleVectors(ent->client->v_angle, forward, right, NULL);
	VectorSet(offset, 24, 8, ent->viewheight - 8);
	VectorAdd(offset, g_offset, offset);
	P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);

	VectorScale(forward, -2, ent->client->kick_origin);
	ent->client->kick_angles[0] = -1;

	fire_supershotgun_alt(ent, start, forward, damage, 1000, effect);
}

//kmw Machinegun Code
void Alt_Machinegun_Explode(edict_t* ent)
{
	vec3_t		origin;
	int			mod;

	if (ent->owner->client)
		PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);

	//FIXME: if we are onground then raise our Z just a bit since we are a point?
	if (ent->enemy)
	{
		float	points;
		vec3_t	v;
		vec3_t	dir;

		VectorAdd(ent->enemy->mins, ent->enemy->maxs, v);
		VectorMA(ent->enemy->s.origin, 0.5, v, v);
		VectorSubtract(ent->s.origin, v, v);
		points = ent->dmg - 0.5 * VectorLength(v);
		VectorSubtract(ent->enemy->s.origin, ent->s.origin, dir);
		if (ent->spawnflags & 1)
			mod = MOD_HANDGRENADE;
		else
			mod = MOD_GRENADE;
		T_Damage(ent->enemy, ent, ent->owner, dir, ent->s.origin, vec3_origin, (int)points, (int)points, DAMAGE_RADIUS, mod);
	}

	if (ent->spawnflags & 2)
		mod = MOD_HELD_GRENADE;
	else if (ent->spawnflags & 1)
		mod = MOD_HG_SPLASH;
	else
		mod = MOD_G_SPLASH;
	T_RadiusDamage(ent, ent->owner, ent->dmg, ent->enemy, ent->dmg_radius, mod);

	VectorMA(ent->s.origin, -0.02, ent->velocity, origin);
	gi.WriteByte(svc_temp_entity);
	if (ent->waterlevel)
	{
		if (ent->groundentity)
			gi.WriteByte(TE_GRENADE_EXPLOSION_WATER);
		else
			gi.WriteByte(TE_ROCKET_EXPLOSION_WATER);
	}
	else
	{
		if (ent->groundentity)
			gi.WriteByte(TE_GRENADE_EXPLOSION);
		else
			gi.WriteByte(TE_ROCKET_EXPLOSION);
	}
	gi.WritePosition(origin);
	gi.multicast(ent->s.origin, MULTICAST_PHS);

	G_FreeEdict(ent);
}

void Alt_Machinegun_Touch(edict_t* ent, edict_t* other, cplane_t* plane, csurface_t* surf)
{
	if (other == ent->owner)
		return;

	if (surf && (surf->flags & SURF_SKY))
	{
		G_FreeEdict(ent);
		return;
	}

	if (!other->takedamage)
	{
		if (ent->spawnflags & 1)
		{
			if (random() > 0.5)
				gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/hgrenb1a.wav"), 1, ATTN_NORM, 0);
			else
				gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/hgrenb2a.wav"), 1, ATTN_NORM, 0);
		}
		else
		{
			gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/grenlb1b.wav"), 1, ATTN_NORM, 0);
		}
		return;
	}

	ent->enemy = other;
	Alt_Machinegun_Explode(ent);
}

void fire_machinegun_alt(edict_t* self, vec3_t start, vec3_t aimdir, int damage, int speed, float timer, float damage_radius)
{
	edict_t* grenade;
	vec3_t	dir;
	vec3_t	forward, right, up;

	vectoangles(aimdir, dir);
	AngleVectors(dir, forward, right, up);

	grenade = G_Spawn();
	VectorCopy(start, grenade->s.origin);
	VectorScale(aimdir, speed, grenade->velocity);
	VectorMA(grenade->velocity, 200 + crandom() * 10.0, up, grenade->velocity);
	VectorMA(grenade->velocity, crandom() * 10.0, right, grenade->velocity);
	VectorSet(grenade->avelocity, 300, 300, 300);
	grenade->movetype = MOVETYPE_BOUNCE;
	grenade->clipmask = MASK_SHOT;
	grenade->solid = SOLID_BBOX;
	grenade->s.effects |= EF_GRENADE;
	VectorClear(grenade->mins);
	VectorClear(grenade->maxs);
	grenade->s.modelindex = gi.modelindex("models/objects/grenade/tris.md2");
	grenade->owner = self;
	grenade->touch = Alt_Machinegun_Touch;
	grenade->nextthink = level.time + timer;
	grenade->think = Alt_Machinegun_Explode;
	grenade->dmg = damage;
	grenade->dmg_radius = damage_radius;
	grenade->classname = "grenade";

	gi.linkentity(grenade);
}

void Alt_MachineGun_Fire(edict_t* ent) {
	vec3_t	offset;
	vec3_t	forward, right;
	vec3_t	start;
	int		damage = 120;
	float	radius;

	radius = damage + 40;

	VectorSet(offset, 8, 8, ent->viewheight - 8);
	AngleVectors(ent->client->v_angle, forward, right, NULL);
	P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);

	VectorScale(forward, -2, ent->client->kick_origin);
	ent->client->kick_angles[0] = -1;

	fire_machinegun_alt(ent, start, forward, damage, 600, 2.5, radius);
}

//kmw Grenade Launcher Code
void Alt_GrenadeLauncher_Explode(edict_t* ent)
{
	vec3_t		origin;
	int			mod;

	if (ent->owner->client)
		PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);

	if (ent->enemy)
	{
		float	points;
		vec3_t	v;
		vec3_t	dir;

		VectorAdd(ent->enemy->mins, ent->enemy->maxs, v);
		VectorMA(ent->enemy->s.origin, 0.5, v, v);
		VectorSubtract(ent->s.origin, v, v);
		points = ent->dmg - 0.5 * VectorLength(v);
		VectorSubtract(ent->enemy->s.origin, ent->s.origin, dir);
		if (ent->spawnflags & 1)
			mod = MOD_HANDGRENADE;
		else
			mod = MOD_GRENADE;
		T_Damage(ent->enemy, ent, ent->owner, dir, ent->s.origin, vec3_origin, (int)points, (int)points, DAMAGE_RADIUS, mod);
	}

	if (ent->spawnflags & 2)
		mod = MOD_HELD_GRENADE;
	else if (ent->spawnflags & 1)
		mod = MOD_HG_SPLASH;
	else
		mod = MOD_G_SPLASH;
	T_RadiusDamage(ent, ent->owner, ent->dmg, ent->enemy, ent->dmg_radius, mod);

	VectorMA(ent->s.origin, -0.02, ent->velocity, origin);
	gi.WriteByte(svc_temp_entity);
	if (ent->waterlevel)
	{
		if (ent->groundentity)
			gi.WriteByte(TE_GRENADE_EXPLOSION_WATER);
		else
			gi.WriteByte(TE_ROCKET_EXPLOSION_WATER);
	}
	else
	{
		if (ent->groundentity)
			gi.WriteByte(TE_GRENADE_EXPLOSION);
		else
			gi.WriteByte(TE_ROCKET_EXPLOSION);
	}
	gi.WritePosition(origin);
	gi.multicast(ent->s.origin, MULTICAST_PHS);

	G_FreeEdict(ent);
	grenadelauncherSkill = 0;
}

void Alt_GrenadeLauncher_Touch(edict_t* ent, edict_t* other, cplane_t* plane, csurface_t* surf)
{
	if (other == ent->owner)
		return;

	if (surf && (surf->flags & SURF_SKY))
	{
		G_FreeEdict(ent);
		return;
	}

	if (!other->takedamage)
	{
		if (ent->spawnflags & 1)
		{
			if (random() > 0.5)
				gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/hgrenb1a.wav"), 1, ATTN_NORM, 0);
			else
				gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/hgrenb2a.wav"), 1, ATTN_NORM, 0);
		}
		else
		{
			gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/grenlb1b.wav"), 1, ATTN_NORM, 0);
		}
		return;
	}

	ent->enemy = other;
	Alt_GrenadeLauncher_Explode(other);
}

void Alt_GrenadeLauncher_Fire(edict_t* ent, vec3_t origin, int flag) {
	//If mine is not set
	if (flag == 0) {
		grenadelauncherSkill = 1;
		grenadelauncherMine = G_Spawn();
		VectorCopy(origin, grenadelauncherMine->s.origin);
		grenadelauncherMine->movetype = MOVETYPE_NONE;
		grenadelauncherMine->clipmask = MASK_SHOT;
		grenadelauncherMine->solid = SOLID_BBOX;
		grenadelauncherMine->s.effects |= EF_GRENADE;
		VectorClear(grenadelauncherMine->mins);
		VectorClear(grenadelauncherMine->maxs);
		grenadelauncherMine->s.modelindex = gi.modelindex("models/objects/grenade/tris.md2");
		grenadelauncherMine->owner = ent;
		grenadelauncherMine->touch = Alt_GrenadeLauncher_Touch;
		grenadelauncherMine->nextthink = level.time + 10;
		grenadelauncherMine->think = Alt_GrenadeLauncher_Explode;
		grenadelauncherMine->dmg = 125;
		grenadelauncherMine->dmg_radius = 160;
		grenadelauncherMine->classname = "grenade";

		gi.linkentity(grenadelauncherMine);

	}
	//If mine is set
	else if (flag == 1) {
		Alt_GrenadeLauncher_Explode(grenadelauncherMine);
	}
}

//kmw Chaingun Code
void fire_chaingun_alt(edict_t* self, vec3_t start, vec3_t aimdir, int damage, int speed, float timer, float damage_radius) {
	edict_t* grenade;
	vec3_t	dir;
	vec3_t	forward, right, up;

	vectoangles(aimdir, dir);
	AngleVectors(dir, forward, right, up);

	grenade = G_Spawn();
	VectorCopy(start, grenade->s.origin);
	VectorScale(aimdir, speed, grenade->velocity);
	VectorMA(grenade->velocity, 200 + crandom() * 10.0, up, grenade->velocity);
	VectorMA(grenade->velocity, crandom() * 10.0, right, grenade->velocity);
	VectorSet(grenade->avelocity, 300, 300, 300);
	grenade->movetype = MOVETYPE_BOUNCE;
	grenade->clipmask = MASK_SHOT;
	grenade->solid = SOLID_BBOX;
	grenade->s.effects |= EF_BFG;
	VectorClear(grenade->mins);
	VectorClear(grenade->maxs);
	grenade->s.modelindex = gi.modelindex("models/objects/grenade/tris.md2");
	grenade->owner = self;
	grenade->touch = Alt_Machinegun_Explode;
	grenade->nextthink = level.time + timer;
	grenade->think = Alt_Machinegun_Explode;
	grenade->dmg = damage;
	grenade->dmg_radius = damage_radius;
	grenade->classname = "grenade";

	gi.linkentity(grenade);
}

void Alt_ChainGun_Fire(edict_t* ent)
{
	vec3_t	offset;
	vec3_t	forward, right;
	vec3_t	start;
	int		damage = 200;
	float	radius;

	radius = damage + 55;

	VectorSet(offset, 8, 8, ent->viewheight - 8);
	AngleVectors(ent->client->v_angle, forward, right, NULL);
	P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);

	VectorScale(forward, -2, ent->client->kick_origin);
	ent->client->kick_angles[0] = -1;
	fire_chaingun_alt(ent, start, forward, damage, 500, 2.0, radius);
}

//kmw BFG Code
void bfg_alt_explode(edict_t* self)
{
	edict_t* ent;
	float	points;
	vec3_t	v;
	float	dist;

	if (self->s.frame == 0)
	{
		// the BFG effect
		ent = NULL;
		while ((ent = findradius(ent, self->s.origin, self->dmg_radius)) != NULL)
		{
			if (!ent->takedamage)
				continue;
			if (ent == self->owner)
				continue;
			if (!CanDamage(ent, self))
				continue;
			if (!CanDamage(ent, self->owner))
				continue;

			VectorAdd(ent->mins, ent->maxs, v);
			VectorMA(ent->s.origin, 0.5, v, v);
			VectorSubtract(self->s.origin, v, v);
			dist = VectorLength(v);
			points = self->radius_dmg * (1.0 - sqrt(dist / self->dmg_radius));
			if (ent == self->owner)
				points = points * 0.5;

			gi.WriteByte(svc_temp_entity);
			gi.WriteByte(TE_BFG_EXPLOSION);
			gi.WritePosition(ent->s.origin);
			gi.multicast(ent->s.origin, MULTICAST_PHS);
			T_Damage(ent, self, self->owner, self->velocity, ent->s.origin, vec3_origin, (int)points, 0, DAMAGE_ENERGY, MOD_BFG_EFFECT);
		}
	}

	self->nextthink = level.time + FRAMETIME;
	self->s.frame++;
	if (self->s.frame == 5)
		self->think = G_FreeEdict;
}

void bfg_alt_think(edict_t* self)
{
	edict_t* ent;
	edict_t* ignore;
	vec3_t	point;
	vec3_t	dir;
	vec3_t	start;
	vec3_t	end;
	int		dmg;
	trace_t	tr;

	dmg = 0;

	ent = NULL;
	while ((ent = findradius(ent, self->s.origin, 256)) != NULL)
	{
		if (ent == self)
			continue;

		if (ent == self->owner)
			continue;

		if (!ent->takedamage)
			continue;

		if (!(ent->svflags & SVF_MONSTER) && (!ent->client) && (strcmp(ent->classname, "misc_explobox") != 0))
			continue;

		VectorMA(ent->absmin, 0.5, ent->size, point);

		VectorSubtract(point, self->s.origin, dir);
		VectorNormalize(dir);

		ignore = self;
		VectorCopy(self->s.origin, start);
		VectorMA(start, 2048, dir, end);
		while (1)
		{
			tr = gi.trace(start, NULL, NULL, end, ignore, CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_DEADMONSTER);

			if (!tr.ent)
				break;

			// hurt it if we can
			if ((tr.ent->takedamage) && !(tr.ent->flags & FL_IMMUNE_LASER) && (tr.ent != self->owner))
				T_Damage(tr.ent, self, self->owner, dir, tr.endpos, vec3_origin, dmg, 1, DAMAGE_ENERGY, MOD_BFG_LASER);

			// if we hit something that's not a monster or player we're done
			if (!(tr.ent->svflags & SVF_MONSTER) && (!tr.ent->client))
			{
				gi.WriteByte(svc_temp_entity);
				gi.WriteByte(TE_LASER_SPARKS);
				gi.WriteByte(4);
				gi.WritePosition(tr.endpos);
				gi.WriteDir(tr.plane.normal);
				gi.WriteByte(self->s.skinnum);
				gi.multicast(tr.endpos, MULTICAST_PVS);
				break;
			}

			ignore = tr.ent;
			VectorCopy(tr.endpos, start);
		}

		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_BFG_LASER);
		gi.WritePosition(self->s.origin);
		gi.WritePosition(tr.endpos);
		gi.multicast(self->s.origin, MULTICAST_PHS);
	}

	self->nextthink = level.time + FRAMETIME;
}

void bfg_alt_touch(edict_t* self, edict_t* other, cplane_t* plane, csurface_t* surf)
{
	if (other == self->owner)
		return;

	if (surf && (surf->flags & SURF_SKY))
	{
		G_FreeEdict(self);
		return;
	}

	if (self->owner->client)
		PlayerNoise(self->owner, self->s.origin, PNOISE_IMPACT);

	// core explosion - prevents firing it into the wall/floor
	if (other->takedamage)
		T_Damage(other, self, self->owner, self->velocity, self->s.origin, plane->normal, 200, 0, 0, MOD_BFG_BLAST);
	T_RadiusDamage(self, self->owner, 200, other, 100, MOD_BFG_BLAST);

	gi.sound(self, CHAN_VOICE, gi.soundindex("weapons/bfg__x1b.wav"), 1, ATTN_NORM, 0);
	self->solid = SOLID_NOT;
	self->touch = NULL;
	VectorMA(self->s.origin, -1 * FRAMETIME, self->velocity, self->s.origin);
	VectorClear(self->velocity);
	self->s.modelindex = gi.modelindex("sprites/s_bfg3.sp2");
	self->s.frame = 0;
	self->s.sound = 0;
	self->s.effects &= ~EF_ANIM_ALLFAST;
	self->think = bfg_alt_explode;
	self->nextthink = level.time + FRAMETIME;
	self->enemy = other;

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_BFG_BIGEXPLOSION);
	gi.WritePosition(self->s.origin);
	gi.multicast(self->s.origin, MULTICAST_PVS);
}

void fire_bfg_alt(edict_t* self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius)
{
	edict_t* bfg;

	bfg = G_Spawn();
	VectorCopy(start, bfg->s.origin);
	VectorCopy(dir, bfg->movedir);
	vectoangles(dir, bfg->s.angles);
	VectorScale(dir, speed, bfg->velocity);
	bfg->movetype = MOVETYPE_FLYMISSILE;
	bfg->clipmask = MASK_SHOT;
	bfg->solid = SOLID_BBOX;
	bfg->s.effects |= EF_BFG | EF_ANIM_ALLFAST;
	VectorClear(bfg->mins);
	VectorClear(bfg->maxs);
	bfg->s.modelindex = gi.modelindex("sprites/s_bfg1.sp2");
	bfg->owner = self;
	bfg->touch = bfg_alt_touch;
	bfg->nextthink = level.time + 8000 / speed;
	bfg->think = G_FreeEdict;
	bfg->radius_dmg = damage;
	bfg->dmg_radius = damage_radius;
	bfg->classname = "bfg blast";
	bfg->s.sound = gi.soundindex("weapons/bfg__l1a.wav");

	bfg->think = bfg_alt_think;
	bfg->nextthink = level.time + FRAMETIME;
	bfg->teammaster = bfg;
	bfg->teamchain = NULL;

	gi.linkentity(bfg);
}

void Alt_BFG_Fire(edict_t* ent, int power)
{
	vec3_t	offset, start;
	vec3_t	forward, right;
	int		damage;
	float	damage_radius = 800 + power;

	damage = 500 + power;

	if (ent->client->ps.gunframe == 9)
	{
		// send muzzle flash
		gi.WriteByte(svc_muzzleflash);
		gi.WriteShort(ent - g_edicts);
		gi.multicast(ent->s.origin, MULTICAST_PVS);

		ent->client->ps.gunframe++;

		PlayerNoise(ent, ent->s.origin, PNOISE_WEAPON);
		return;
	}

	AngleVectors(ent->client->v_angle, forward, right, NULL);

	VectorScale(forward, -2, ent->client->kick_origin);

	// make a big pitch kick with an inverse fall
	ent->client->v_dmg_pitch = -40;
	ent->client->v_dmg_roll = crandom() * 8;
	ent->client->v_dmg_time = level.time + DAMAGE_TIME;

	VectorSet(offset, 8, 8, ent->viewheight - 8);
	P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);
	fire_bfg_alt(ent, start, forward, damage, 100 + power, damage_radius);

	ent->client->ps.gunframe++;

	PlayerNoise(ent, start, PNOISE_WEAPON);
}

//kmw RailGun Code
void Alt_RailGun_Think(edict_t* ent) {
	gi.centerprintf(ent->owner, "Teleporter has disappeared");
	G_FreeEdict(ent);
	railgunSkill = 0;
}

void Alt_RailGun_Fire(edict_t* ent, vec3_t origin, int flag) {
	//If TP is not set
	if (flag == 0) {
		railgunSkill = 1;
		teleporter = G_Spawn();
		VectorCopy(origin, teleporter->s.origin);
		teleporter->movetype = MOVETYPE_NONE;
		teleporter->clipmask = MASK_SHOT;
		teleporter->solid = SOLID_BBOX;
		teleporter->s.effects |= EF_GRENADE;
		VectorClear(teleporter->mins);
		VectorClear(teleporter->maxs);
		teleporter->s.modelindex = gi.modelindex("models/objects/grenade/tris.md2");
		teleporter->owner = ent;
		teleporter->nextthink = level.time + 60;
		teleporter->think = Alt_RailGun_Think;
		teleporter->dmg = 125;
		teleporter->dmg_radius = 160;
		teleporter->classname = "grenade";

		gi.linkentity(teleporter);

	}
	//If TP is set
	else if (flag == 1) {
		VectorCopy(teleporter->s.origin, ent->s.origin);
		VectorCopy(teleporter->s.origin, ent->s.old_origin);
		G_FreeEdict(teleporter);
		railgunSkill = 0;
	}
}

//RocketLauncher Code
void alt_rocket_think(edict_t* self) {
	edict_t* ent;
	edict_t* ignore;
	vec3_t	point;
	vec3_t	dir;
	vec3_t	start;
	vec3_t	end;
	int		dmg;
	trace_t	tr;

	dmg = 10;

	if (level.time > rocketlauncherCooldown) {
		G_FreeEdict(self);
		rocketlauncherSkill = 0;
		rocketlauncherCooldown = 0.0f;
		return;
	}

	ent = NULL;
	while ((ent = findradius(ent, self->s.origin, 100)) != NULL)
	{
		if (ent == self)
			continue;

		if (ent == self->owner)
			continue;

		if (!ent->takedamage)
			continue;

		if (!(ent->svflags & SVF_MONSTER) && (!ent->client) && (strcmp(ent->classname, "misc_explobox") != 0))
			continue;

		VectorMA(ent->absmin, 0.5, ent->size, point);

		VectorSubtract(point, self->s.origin, dir);
		VectorNormalize(dir);

		ignore = self;
		VectorCopy(self->s.origin, start);
		VectorMA(start, 2048, dir, end);
		while (1)
		{
			tr = gi.trace(start, NULL, NULL, end, ignore, CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_DEADMONSTER);

			if (!tr.ent)
					break;

			// hurt it if we can
			if ((tr.ent->takedamage) && !(tr.ent->flags & FL_IMMUNE_LASER) && (tr.ent != self->owner))
					T_Damage(tr.ent, self, self->owner, dir, tr.endpos, vec3_origin, dmg, 1, DAMAGE_ENERGY, MOD_BFG_LASER);

			// if we hit something that's not a monster or player we're done
			if (!(tr.ent->svflags & SVF_MONSTER) && (!tr.ent->client))
			{
				gi.WriteByte(svc_temp_entity);
				gi.WriteByte(TE_LASER_SPARKS);
				gi.WriteByte(4);
				gi.WritePosition(tr.endpos);
				gi.WriteDir(tr.plane.normal);
				gi.WriteByte(self->s.skinnum);
				gi.multicast(tr.endpos, MULTICAST_PVS);
				break;
			}

			ignore = tr.ent;
			VectorCopy(tr.endpos, start);
		}

		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_BFG_LASER);
		gi.WritePosition(self->s.origin);
		gi.WritePosition(tr.endpos);
		gi.multicast(self->s.origin, MULTICAST_PHS);
	}

	self->nextthink = level.time + FRAMETIME;
}




void alt_rocket_touch(edict_t* ent, edict_t* other, cplane_t* plane, csurface_t* surf)
{
	vec3_t		origin;
	int			n;

	if (other == ent->owner)
		return;

	if (surf && (surf->flags & SURF_SKY))
	{
		G_FreeEdict(ent);
		return;
	}

	if (ent->owner->client)
		PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);

	// calculate position for the explosion entity
	VectorMA(ent->s.origin, -0.02, ent->velocity, origin);

	if (other->takedamage)
	{
		T_Damage(other, ent, ent->owner, ent->velocity, ent->s.origin, plane->normal, ent->dmg, 0, 0, MOD_ROCKET);
	}
	else
	{
		// don't throw any debris in net games
		if (!deathmatch->value && !coop->value)
		{
			if ((surf) && !(surf->flags & (SURF_WARP | SURF_TRANS33 | SURF_TRANS66 | SURF_FLOWING)))
			{
				n = rand() % 5;
				while (n--)
					ThrowDebris(ent, "models/objects/debris2/tris.md2", 2, ent->s.origin);
			}
		}
	}

	T_RadiusDamage(ent, ent->owner, ent->radius_dmg, other, ent->dmg_radius, MOD_R_SPLASH);

	gi.WriteByte(svc_temp_entity);
	if (ent->waterlevel)
		gi.WriteByte(TE_ROCKET_EXPLOSION_WATER);
	else
		gi.WriteByte(TE_ROCKET_EXPLOSION);
	gi.WritePosition(origin);
	gi.multicast(ent->s.origin, MULTICAST_PHS);

	G_FreeEdict(ent);
	rocketlauncherSkill = 0;
	rocketlauncherCooldown = 0.0f;
}

void alt_fire_rocket(edict_t* self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius, int radius_damage) {
	edict_t* rocket;

	rocket = G_Spawn();
	VectorCopy(start, rocket->s.origin);
	VectorCopy(dir, rocket->movedir);
	vectoangles(dir, rocket->s.angles);
	VectorScale(dir, speed, rocket->velocity);
	rocket->movetype = MOVETYPE_FLYMISSILE;
	rocket->clipmask = MASK_SHOT;
	rocket->solid = SOLID_BBOX;
	rocket->s.effects |= EF_ROCKET;
	VectorClear(rocket->mins);
	VectorClear(rocket->maxs);
	rocket->s.modelindex = gi.modelindex("models/objects/rocket/tris.md2");
	rocket->owner = self;
	rocket->touch = alt_rocket_touch;
	rocket->nextthink = level.time + FRAMETIME;
	rocket->think = alt_rocket_think;
	rocket->dmg = damage;
	rocket->radius_dmg = radius_damage;
	rocket->dmg_radius = damage_radius;
	rocket->s.sound = gi.soundindex("weapons/rockfly.wav");
	rocket->classname = "rocket";

	gi.linkentity(rocket);
}


void Alt_Rocket_Fire(edict_t* ent) {
	vec3_t	offset, start;
	vec3_t	forward, right;
	int		damage;
	float	damage_radius;
	int		radius_damage;

	damage = 100 + (int)(random() * 20.0);
	radius_damage = 120;
	damage_radius = 120;

	AngleVectors(ent->client->v_angle, forward, right, NULL);

	VectorScale(forward, -2, ent->client->kick_origin);
	ent->client->kick_angles[0] = -1;

	VectorSet(offset, 8, 8, ent->viewheight - 8);
	P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);
	alt_fire_rocket(ent, start, forward, damage, 200, damage_radius, radius_damage);
}

//=================================================================================

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
void Cmd_Give_f (edict_t *ent)
{
	char		*name;
	gitem_t		*it;
	int			index;
	int			i;
	qboolean	give_all;
	edict_t		*it_ent;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	name = gi.args();

	if (Q_stricmp(name, "all") == 0)
		give_all = true;
	else
		give_all = false;

	if (give_all || Q_stricmp(gi.argv(1), "health") == 0)
	{
		if (gi.argc() == 3)
			ent->health = atoi(gi.argv(2));
		else
			ent->health = ent->max_health;
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "weapons") == 0)
	{
		for (i=0 ; i<game.num_items ; i++)
		{
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (!(it->flags & IT_WEAPON))
				continue;
			ent->client->pers.inventory[i] += 1;
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "ammo") == 0)
	{
		for (i=0 ; i<game.num_items ; i++)
		{
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (!(it->flags & IT_AMMO))
				continue;
			Add_Ammo (ent, it, 1000);
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "armor") == 0)
	{
		gitem_armor_t	*info;

		it = FindItem("Jacket Armor");
		ent->client->pers.inventory[ITEM_INDEX(it)] = 0;

		it = FindItem("Combat Armor");
		ent->client->pers.inventory[ITEM_INDEX(it)] = 0;

		it = FindItem("Body Armor");
		info = (gitem_armor_t *)it->info;
		ent->client->pers.inventory[ITEM_INDEX(it)] = info->max_count;

		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "Power Shield") == 0)
	{
		it = FindItem("Power Shield");
		it_ent = G_Spawn();
		it_ent->classname = it->classname;
		SpawnItem (it_ent, it);
		Touch_Item (it_ent, ent, NULL, NULL);
		if (it_ent->inuse)
			G_FreeEdict(it_ent);

		if (!give_all)
			return;
	}

	if (give_all)
	{
		for (i=0 ; i<game.num_items ; i++)
		{
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (it->flags & (IT_ARMOR|IT_WEAPON|IT_AMMO))
				continue;
			ent->client->pers.inventory[i] = 1;
		}
		return;
	}

	it = FindItem (name);
	if (!it)
	{
		name = gi.argv(1);
		it = FindItem (name);
		if (!it)
		{
			gi.cprintf (ent, PRINT_HIGH, "unknown item\n");
			return;
		}
	}

	if (!it->pickup)
	{
		gi.cprintf (ent, PRINT_HIGH, "non-pickup item\n");
		return;
	}

	index = ITEM_INDEX(it);

	if (it->flags & IT_AMMO)
	{
		if (gi.argc() == 3)
			ent->client->pers.inventory[index] = atoi(gi.argv(2));
		else
			ent->client->pers.inventory[index] += it->quantity;
	}
	else
	{
		it_ent = G_Spawn();
		it_ent->classname = it->classname;
		SpawnItem (it_ent, it);
		Touch_Item (it_ent, ent, NULL, NULL);
		if (it_ent->inuse)
			G_FreeEdict(it_ent);
	}
}


/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f (edict_t *ent)
{
	char	*msg;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	ent->flags ^= FL_GODMODE;
	if (!(ent->flags & FL_GODMODE) )
		msg = "godmode OFF\n";
	else
		msg = "godmode ON\n";

	gi.cprintf (ent, PRINT_HIGH, msg);
}


/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f (edict_t *ent)
{
	char	*msg;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	ent->flags ^= FL_NOTARGET;
	if (!(ent->flags & FL_NOTARGET) )
		msg = "notarget OFF\n";
	else
		msg = "notarget ON\n";

	gi.cprintf (ent, PRINT_HIGH, msg);
}


/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f (edict_t *ent)
{
	char	*msg;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	if (ent->movetype == MOVETYPE_NOCLIP)
	{
		ent->movetype = MOVETYPE_WALK;
		msg = "noclip OFF\n";
	}
	else
	{
		ent->movetype = MOVETYPE_NOCLIP;
		msg = "noclip ON\n";
	}

	gi.cprintf (ent, PRINT_HIGH, msg);
}


/*
==================
Cmd_Use_f

Use an inventory item
==================
*/
void Cmd_Use_f (edict_t *ent)
{
	int			index;
	gitem_t		*it;
	char		*s;

	s = gi.args();
	it = FindItem (s);
	if (!it)
	{
		gi.cprintf (ent, PRINT_HIGH, "unknown item: %s\n", s);
		return;
	}
	if (!it->use)
	{
		gi.cprintf (ent, PRINT_HIGH, "Item is not usable.\n");
		return;
	}
	index = ITEM_INDEX(it);
	if (!ent->client->pers.inventory[index])
	{
		gi.cprintf (ent, PRINT_HIGH, "Out of item: %s\n", s);
		return;
	}

	it->use (ent, it);
}


/*
==================
Cmd_Drop_f

Drop an inventory item
==================
*/
void Cmd_Drop_f (edict_t *ent)
{
	int			index;
	gitem_t		*it;
	char		*s;

	s = gi.args();
	it = FindItem (s);
	if (!it)
	{
		gi.cprintf (ent, PRINT_HIGH, "unknown item: %s\n", s);
		return;
	}
	if (!it->drop)
	{
		gi.cprintf (ent, PRINT_HIGH, "Item is not dropable.\n");
		return;
	}
	index = ITEM_INDEX(it);
	if (!ent->client->pers.inventory[index])
	{
		gi.cprintf (ent, PRINT_HIGH, "Out of item: %s\n", s);
		return;
	}

	it->drop (ent, it);
}


/*
=================
Cmd_Inven_f
=================
*/
void Cmd_Inven_f (edict_t *ent)
{
	int			i;
	gclient_t	*cl;

	cl = ent->client;

	cl->showscores = false;
	cl->showhelp = false;

	if (cl->showinventory)
	{
		cl->showinventory = false;
		return;
	}

	cl->showinventory = true;

	gi.WriteByte (svc_inventory);
	for (i=0 ; i<MAX_ITEMS ; i++)
	{
		gi.WriteShort (cl->pers.inventory[i]);
	}
	gi.unicast (ent, true);
}


/*
=================
Cmd_InvUse_f
=================
*/
void Cmd_InvUse_f (edict_t *ent)
{
	gitem_t		*it;

	ValidateSelectedItem (ent);

	if (ent->client->pers.selected_item == -1)
	{
		gi.cprintf (ent, PRINT_HIGH, "No item to use.\n");
		return;
	}

	it = &itemlist[ent->client->pers.selected_item];
	if (!it->use)
	{
		gi.cprintf (ent, PRINT_HIGH, "Item is not usable.\n");
		return;
	}
	it->use (ent, it);
}

/*
=================
Cmd_WeapPrev_f
=================
*/
void Cmd_WeapPrev_f (edict_t *ent)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;
	int			selected_weapon;

	cl = ent->client;

	if (!cl->pers.weapon)
		return;

	selected_weapon = ITEM_INDEX(cl->pers.weapon);

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (selected_weapon + i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (! (it->flags & IT_WEAPON) )
			continue;
		it->use (ent, it);
		if (cl->pers.weapon == it)
			return;	// successful
	}
}

/*
=================
Cmd_WeapNext_f
=================
*/
void Cmd_WeapNext_f (edict_t *ent)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;
	int			selected_weapon;

	cl = ent->client;

	if (!cl->pers.weapon)
		return;

	selected_weapon = ITEM_INDEX(cl->pers.weapon);

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (selected_weapon + MAX_ITEMS - i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (! (it->flags & IT_WEAPON) )
			continue;
		it->use (ent, it);
		if (cl->pers.weapon == it)
			return;	// successful
	}
}

/*
=================
Cmd_WeapLast_f
=================
*/
void Cmd_WeapLast_f (edict_t *ent)
{
	gclient_t	*cl;
	int			index;
	gitem_t		*it;

	cl = ent->client;

	if (!cl->pers.weapon || !cl->pers.lastweapon)
		return;

	index = ITEM_INDEX(cl->pers.lastweapon);
	if (!cl->pers.inventory[index])
		return;
	it = &itemlist[index];
	if (!it->use)
		return;
	if (! (it->flags & IT_WEAPON) )
		return;
	it->use (ent, it);
}

/*
=================
Cmd_InvDrop_f
=================
*/
void Cmd_InvDrop_f (edict_t *ent)
{
	gitem_t		*it;

	ValidateSelectedItem (ent);

	if (ent->client->pers.selected_item == -1)
	{
		gi.cprintf (ent, PRINT_HIGH, "No item to drop.\n");
		return;
	}

	it = &itemlist[ent->client->pers.selected_item];
	if (!it->drop)
	{
		gi.cprintf (ent, PRINT_HIGH, "Item is not dropable.\n");
		return;
	}
	it->drop (ent, it);
}

/*
=================
Cmd_Kill_f
=================
*/
void Cmd_Kill_f (edict_t *ent)
{
	if((level.time - ent->client->respawn_time) < 5)
		return;
	ent->flags &= ~FL_GODMODE;
	ent->health = 0;
	meansOfDeath = MOD_SUICIDE;
	player_die (ent, ent, ent, 100000, vec3_origin);
}

/*
=================
Cmd_PutAway_f
=================
*/
void Cmd_PutAway_f (edict_t *ent)
{
	ent->client->showscores = false;
	ent->client->showhelp = false;
	ent->client->showinventory = false;
}


int PlayerSort (void const *a, void const *b)
{
	int		anum, bnum;

	anum = *(int *)a;
	bnum = *(int *)b;

	anum = game.clients[anum].ps.stats[STAT_FRAGS];
	bnum = game.clients[bnum].ps.stats[STAT_FRAGS];

	if (anum < bnum)
		return -1;
	if (anum > bnum)
		return 1;
	return 0;
}

/*
=================
Cmd_Players_f
=================
*/
void Cmd_Players_f (edict_t *ent)
{
	int		i;
	int		count;
	char	small[64];
	char	large[1280];
	int		index[256];

	count = 0;
	for (i = 0 ; i < maxclients->value ; i++)
		if (game.clients[i].pers.connected)
		{
			index[count] = i;
			count++;
		}

	// sort by frags
	qsort (index, count, sizeof(index[0]), PlayerSort);

	// print information
	large[0] = 0;

	for (i = 0 ; i < count ; i++)
	{
		Com_sprintf (small, sizeof(small), "%3i %s\n",
			game.clients[index[i]].ps.stats[STAT_FRAGS],
			game.clients[index[i]].pers.netname);
		if (strlen (small) + strlen(large) > sizeof(large) - 100 )
		{	// can't print all of them in one packet
			strcat (large, "...\n");
			break;
		}
		strcat (large, small);
	}

	gi.cprintf (ent, PRINT_HIGH, "%s\n%i players\n", large, count);
}

/*
=================
Cmd_Wave_f
=================
*/
void Cmd_Wave_f (edict_t *ent)
{
	int		i;

	i = atoi (gi.argv(1));

	// can't wave when ducked
	if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
		return;

	if (ent->client->anim_priority > ANIM_WAVE)
		return;

	ent->client->anim_priority = ANIM_WAVE;

	switch (i)
	{
	case 0:
		gi.cprintf (ent, PRINT_HIGH, "flipoff\n");
		ent->s.frame = FRAME_flip01-1;
		ent->client->anim_end = FRAME_flip12;
		break;
	case 1:
		gi.cprintf (ent, PRINT_HIGH, "salute\n");
		ent->s.frame = FRAME_salute01-1;
		ent->client->anim_end = FRAME_salute11;
		break;
	case 2:
		gi.cprintf (ent, PRINT_HIGH, "taunt\n");
		ent->s.frame = FRAME_taunt01-1;
		ent->client->anim_end = FRAME_taunt17;
		break;
	case 3:
		gi.cprintf (ent, PRINT_HIGH, "wave\n");
		ent->s.frame = FRAME_wave01-1;
		ent->client->anim_end = FRAME_wave11;
		break;
	case 4:
	default:
		gi.cprintf (ent, PRINT_HIGH, "point\n");
		ent->s.frame = FRAME_point01-1;
		ent->client->anim_end = FRAME_point12;
		break;
	}
}

/*
==================
Cmd_Say_f
==================
*/
void Cmd_Say_f (edict_t *ent, qboolean team, qboolean arg0)
{
	int		i, j;
	edict_t	*other;
	char	*p;
	char	text[2048];
	gclient_t *cl;

	if (gi.argc () < 2 && !arg0)
		return;

	if (!((int)(dmflags->value) & (DF_MODELTEAMS | DF_SKINTEAMS)))
		team = false;

	if (team)
		Com_sprintf (text, sizeof(text), "(%s): ", ent->client->pers.netname);
	else
		Com_sprintf (text, sizeof(text), "%s: ", ent->client->pers.netname);

	if (arg0)
	{
		strcat (text, gi.argv(0));
		strcat (text, " ");
		strcat (text, gi.args());
	}
	else
	{
		p = gi.args();

		if (*p == '"')
		{
			p++;
			p[strlen(p)-1] = 0;
		}
		strcat(text, p);
	}

	// don't let text be too long for malicious reasons
	if (strlen(text) > 150)
		text[150] = 0;

	strcat(text, "\n");

	if (flood_msgs->value) {
		cl = ent->client;

        if (level.time < cl->flood_locktill) {
			gi.cprintf(ent, PRINT_HIGH, "You can't talk for %d more seconds\n",
				(int)(cl->flood_locktill - level.time));
            return;
        }
        i = cl->flood_whenhead - flood_msgs->value + 1;
        if (i < 0)
            i = (sizeof(cl->flood_when)/sizeof(cl->flood_when[0])) + i;
		if (cl->flood_when[i] && 
			level.time - cl->flood_when[i] < flood_persecond->value) {
			cl->flood_locktill = level.time + flood_waitdelay->value;
			gi.cprintf(ent, PRINT_CHAT, "Flood protection:  You can't talk for %d seconds.\n",
				(int)flood_waitdelay->value);
            return;
        }
		cl->flood_whenhead = (cl->flood_whenhead + 1) %
			(sizeof(cl->flood_when)/sizeof(cl->flood_when[0]));
		cl->flood_when[cl->flood_whenhead] = level.time;
	}

	if (dedicated->value)
		gi.cprintf(NULL, PRINT_CHAT, "%s", text);

	for (j = 1; j <= game.maxclients; j++)
	{
		other = &g_edicts[j];
		if (!other->inuse)
			continue;
		if (!other->client)
			continue;
		if (team)
		{
			if (!OnSameTeam(ent, other))
				continue;
		}
		gi.cprintf(other, PRINT_CHAT, "%s", text);
	}
}

void Cmd_PlayerList_f(edict_t *ent)
{
	int i;
	char st[80];
	char text[1400];
	edict_t *e2;

	// connect time, ping, score, name
	*text = 0;
	for (i = 0, e2 = g_edicts + 1; i < maxclients->value; i++, e2++) {
		if (!e2->inuse)
			continue;

		sprintf(st, "%02d:%02d %4d %3d %s%s\n",
			(level.framenum - e2->client->resp.enterframe) / 600,
			((level.framenum - e2->client->resp.enterframe) % 600)/10,
			e2->client->ping,
			e2->client->resp.score,
			e2->client->pers.netname,
			e2->client->resp.spectator ? " (spectator)" : "");
		if (strlen(text) + strlen(st) > sizeof(text) - 50) {
			sprintf(text+strlen(text), "And more...\n");
			gi.cprintf(ent, PRINT_HIGH, "%s", text);
			return;
		}
		strcat(text, st);
	}
	gi.cprintf(ent, PRINT_HIGH, "%s", text);
}

//kmw Custom Commands

void Cmd_WeaponAlternate(edict_t* ent)
{
	//Blaster Skill: High Jump
	if (ent->client->pers.weapon->classname == "weapon_blaster") {
		if (blasterSkill == 0) {
			ent->velocity[2] = 420;
			blasterSkill = 1;
		}
	}

	//Shotgun Skill: Dash
	if (ent->client->pers.weapon->classname == "weapon_shotgun") {

		if ((shotgunSkill == 0) && (ent->client->pers.inventory[ent->client->ammo_index] >= 10)) {
			float viewPitch = ent->client->ps.viewangles[PITCH];
			float viewYaw = ent->client->ps.viewangles[YAW];
			//gi.centerprintf(ent, "pitch: %f / yaw: %f", viewPitch, viewYaw);

			if (viewPitch < 0) {
				ent->velocity[2] = (viewPitch * -1) + 300;
			}
			else if (viewPitch > 0) {
				ent->velocity[2] = ((viewPitch * -1) + 300) * -1;
			}
			if (viewYaw < 0) {
				ent->velocity[1] = -500;
			}
			else if (viewYaw > 0) {
				ent->velocity[1] = 500;
			}
			if ((viewYaw < 0) && (viewYaw > -90)) {					// -90 to 0
				if (!(((viewYaw > -90) && (viewYaw < -80)) || ((viewYaw > -10) && (viewYaw < 0))))
					ent->velocity[0] = ((viewYaw * -1) + 300);
			}
			else if ((viewYaw > 0) && (viewYaw < 90)) {				// 0 to 90
				if (!(((viewYaw > 0) && (viewYaw < 10)) || ((viewYaw > 80) && (viewYaw < 90))))
					ent->velocity[0] = (viewYaw + 300);
			}
			else if ((viewYaw < -90) && (viewYaw > -180)) {			//	-90 to -180
				if (!(((viewYaw > -180) && (viewYaw < -170)) || ((viewYaw > -110) && (viewYaw < -90))))
					ent->velocity[0] = ((viewYaw * -1) + 300) * -1;
			}
			else if ((viewYaw > 90) && (viewYaw < 180)) {			// 90 to 180
				if (!(((viewYaw > 90) && (viewYaw < 100)) || ((viewYaw > 170) && (viewYaw < 180))))
					ent->velocity[0] = (viewYaw + 300) * -1;
			}

			shotgunSkill = 1;
			ent->client->pers.inventory[ent->client->ammo_index] -= 10;
			//gi.centerprintf(ent, "current ammo: %i", ent->client->pers.inventory[ent->client->ammo_index]);

			if (ent->client->pers.inventory[ent->client->ammo_index] < 0)
				ent->client->pers.inventory[ent->client->ammo_index] = 0;
		}
		else if (ent->client->pers.inventory[ent->client->ammo_index] < 10) {
			gi.centerprintf(ent, "Not enough ammo!");
		}
	}

	//Super Shotgun Skill: Teleport Bolt
	if (ent->client->pers.weapon->classname == "weapon_supershotgun") {
		vec3_t vec3_origin = { 0,0,0 };

		if (ent->client->pers.inventory[ent->client->ammo_index] >= 20) {
			Alt_SuperShotgun_Fire(ent, vec3_origin, 10000, false, EF_BLASTER);
			ent->client->pers.inventory[ent->client->ammo_index] -= 20;

			if (ent->client->pers.inventory[ent->client->ammo_index] < 0)
				ent->client->pers.inventory[ent->client->ammo_index] = 0;
		}
		else if (ent->client->pers.inventory[ent->client->ammo_index] < 20) {
			gi.centerprintf(ent, "Not enough ammo!");
		}
		
	}

	//Machine Gun Skill: Grenade
	if (ent->client->pers.weapon->classname == "weapon_machinegun") {

		if (ent->client->pers.inventory[ent->client->ammo_index] >= 30) {
			Alt_MachineGun_Fire(ent);
			ent->client->pers.inventory[ent->client->ammo_index] -= 30;

			if (ent->client->pers.inventory[ent->client->ammo_index] < 0)
				ent->client->pers.inventory[ent->client->ammo_index] = 0;
		}
		else if (ent->client->pers.inventory[ent->client->ammo_index] < 30) {
			gi.centerprintf(ent, "Not enough ammo!");
		}
	}

	//Chain Gun Skill: Super Grenade
	if (ent->client->pers.weapon->classname == "weapon_chaingun") {

		if (ent->client->pers.inventory[ent->client->ammo_index] >= 60) {
			Alt_ChainGun_Fire(ent);
			ent->client->pers.inventory[ent->client->ammo_index] -= 60;

			if (ent->client->pers.inventory[ent->client->ammo_index] < 0)
				ent->client->pers.inventory[ent->client->ammo_index] = 0;
		}
		else if (ent->client->pers.inventory[ent->client->ammo_index] < 60) {
			gi.centerprintf(ent, "Not enough ammo!");
		}
	}

	//Grenade Launcher Skill: Mine
	if (ent->client->pers.weapon->classname == "weapon_grenadelauncher") {
		vec3_t mine_origin;
		VectorCopy(ent->s.origin, mine_origin);
		
		if (ent->client->pers.inventory[ent->client->ammo_index] >= 10) {
			if (grenadelauncherSkill == 0) {
				Alt_GrenadeLauncher_Fire(ent, mine_origin, 0);
				ent->client->pers.inventory[ent->client->ammo_index] -= 10;
				gi.centerprintf(ent, "Mine placed");
			}
			else if (grenadelauncherSkill == 1)
				Alt_GrenadeLauncher_Fire(ent, mine_origin, 1);
			
			if (ent->client->pers.inventory[ent->client->ammo_index] < 0)
				ent->client->pers.inventory[ent->client->ammo_index] = 0;
		}
		else if (ent->client->pers.inventory[ent->client->ammo_index] < 10) {
			if (grenadelauncherSkill == 1)
				Alt_GrenadeLauncher_Fire(ent, mine_origin, 1);
			else
				gi.centerprintf(ent, "Not enough ammo!");
		}	
	}

	//BFG Skill: Full Energy Shot
	if (ent->client->pers.weapon->classname == "weapon_bfg") {
		if (ent->client->pers.inventory[ent->client->ammo_index] > 0) {
			int ammo = ent->client->pers.inventory[ent->client->ammo_index];
			Alt_BFG_Fire(ent, ammo);

			ent->client->pers.inventory[ent->client->ammo_index] = 0;
		}
		else
			gi.centerprintf(ent, "Not enough ammo!");
	}

	//HyperBlaster Skill: Healing Shots
	if (ent->client->pers.weapon->classname == "weapon_hyperblaster") {
		if (ent->client->pers.inventory[ent->client->ammo_index] > 0) {
			if (hyperblasterSkill == 0) {
				hyperblasterSkill = 1;
				gi.centerprintf(ent, "Healing Bullets ON");
				hyperblasterCooldown = level.time + 0.5f;
			}
				
			else {
				hyperblasterSkill = 0;
				gi.centerprintf(ent, "Healing Bullets OFF");
			}
		}
	}

	//RailGun Skill: Teleporter
	if (ent->client->pers.weapon->classname == "weapon_railgun") {
		vec3_t tp_origin;
		VectorCopy(ent->s.origin, tp_origin);

		if (ent->client->pers.inventory[ent->client->ammo_index] >= 10) {
			if (railgunSkill == 0) {
				Alt_RailGun_Fire(ent, tp_origin, 0);
				ent->client->pers.inventory[ent->client->ammo_index] -= 10;
				gi.centerprintf(ent, "Teleporter placed");
			}
			else if (railgunSkill == 1)
				Alt_RailGun_Fire(ent, tp_origin, 1);

			if (ent->client->pers.inventory[ent->client->ammo_index] < 0)
				ent->client->pers.inventory[ent->client->ammo_index] = 0;
		}
		else if (ent->client->pers.inventory[ent->client->ammo_index] < 10) {
			if (railgunSkill == 1)
				Alt_RailGun_Fire(ent, tp_origin, 1);
			else
				gi.centerprintf(ent, "Not enough ammo!");
		}
	}

	//RocketLauncher Skill: BFG Rocket
	if (ent->client->pers.weapon->classname == "weapon_rocketlauncher") {
		if (ent->client->pers.inventory[ent->client->ammo_index] >= 10) {
			if (rocketlauncherSkill == 0) {
				Alt_Rocket_Fire(ent);
				rocketlauncherCooldown = level.time + 10000;
				rocketlauncherSkill = 1;
				ent->client->pers.inventory[ent->client->ammo_index] -= 10;
			}
			
			if (ent->client->pers.inventory[ent->client->ammo_index] < 0)
				ent->client->pers.inventory[ent->client->ammo_index] = 0;
		}

		else if (ent->client->pers.inventory[ent->client->ammo_index] < 10) {
			gi.centerprintf(ent, "Not enough ammo!");
		}
	}

}


//kmw Wave Handling

void Cmd_ModStart(edict_t* ent)
{
	if (gameActive == 0) {
		//Initialize game start
		waveNumber = 1;
		money = 0;
		waveTimer = 0;
		enemyCount = 0;
		waveTimerUpdater = level.time + 1;
		shopActive = 0;
		gameActive = 1;
		waveActive = 1;
	}
	else if (gameActive == 1) {
		waveTimer = 0;
		waveTimerUpdater = level.time + 1;
		shopActive = 0;
		waveActive = 1;
	}
	
	WaveSpawn(ent, waveNumber);
}

void Cmd_ModBuy(edict_t* ent)
{
	if ((gameActive == 1) && (shopActive == 1)) {

		if (Q_stricmp(gi.args(1), "heal") == 0) {
			if (money >= 30) {
				ent->health = ent->max_health;
				money -= 30;
				if (money < 0) {
					money = 0;
				}
			}
			else {
				gi.centerprintf(ent, "Not enough money!");
			}
				
		}



	}
}

/*
=================
ClientCommand
=================
*/
void ClientCommand (edict_t *ent)
{
	char	*cmd;

	if (!ent->client)
		return;		// not fully in game yet

	cmd = gi.argv(0);

	if (Q_stricmp (cmd, "players") == 0)
	{
		Cmd_Players_f (ent);
		return;
	}
	if (Q_stricmp (cmd, "say") == 0)
	{
		Cmd_Say_f (ent, false, false);
		return;
	}
	if (Q_stricmp (cmd, "say_team") == 0)
	{
		Cmd_Say_f (ent, true, false);
		return;
	}
	if (Q_stricmp (cmd, "score") == 0)
	{
		Cmd_Score_f (ent);
		return;
	}
	if (Q_stricmp (cmd, "help") == 0)
	{
		Cmd_Help_f (ent);
		return;
	}

	if (level.intermissiontime)
		return;

	if (Q_stricmp(cmd, "use") == 0)
		Cmd_Use_f(ent);
	else if (Q_stricmp(cmd, "drop") == 0)
		Cmd_Drop_f(ent);
	else if (Q_stricmp(cmd, "give") == 0)
		Cmd_Give_f(ent);
	else if (Q_stricmp(cmd, "god") == 0)
		Cmd_God_f(ent);
	else if (Q_stricmp(cmd, "notarget") == 0)
		Cmd_Notarget_f(ent);
	else if (Q_stricmp(cmd, "noclip") == 0)
		Cmd_Noclip_f(ent);
	else if (Q_stricmp(cmd, "inven") == 0)
		Cmd_Inven_f(ent);
	else if (Q_stricmp(cmd, "invnext") == 0)
		SelectNextItem(ent, -1);
	else if (Q_stricmp(cmd, "invprev") == 0)
		SelectPrevItem(ent, -1);
	else if (Q_stricmp(cmd, "invnextw") == 0)
		SelectNextItem(ent, IT_WEAPON);
	else if (Q_stricmp(cmd, "invprevw") == 0)
		SelectPrevItem(ent, IT_WEAPON);
	else if (Q_stricmp(cmd, "invnextp") == 0)
		SelectNextItem(ent, IT_POWERUP);
	else if (Q_stricmp(cmd, "invprevp") == 0)
		SelectPrevItem(ent, IT_POWERUP);
	else if (Q_stricmp(cmd, "invuse") == 0)
		Cmd_InvUse_f(ent);
	else if (Q_stricmp(cmd, "invdrop") == 0)
		Cmd_InvDrop_f(ent);
	else if (Q_stricmp(cmd, "weapprev") == 0)
		Cmd_WeapPrev_f(ent);
	else if (Q_stricmp(cmd, "weapnext") == 0)
		Cmd_WeapNext_f(ent);
	else if (Q_stricmp(cmd, "weaplast") == 0)
		Cmd_WeapLast_f(ent);
	else if (Q_stricmp(cmd, "kill") == 0)
		Cmd_Kill_f(ent);
	else if (Q_stricmp(cmd, "putaway") == 0)
		Cmd_PutAway_f(ent);
	else if (Q_stricmp(cmd, "wave") == 0)
		Cmd_Wave_f(ent);
	else if (Q_stricmp(cmd, "playerlist") == 0)
		Cmd_PlayerList_f(ent);
	//kmw Added commands
	else if (Q_stricmp(cmd, "weaponAlternate") == 0)
		Cmd_WeaponAlternate(ent);
	else if (Q_stricmp(cmd, "start") == 0)
		Cmd_ModStart(ent);
	else if (Q_stricmp(cmd, "buy") == 0)
		Cmd_ModBuy(ent);
	else if (Q_stricmp(cmd, "shop") == 0)
		ShopMenu(ent);
	else	// anything that doesn't match a command will be a chat
		Cmd_Say_f (ent, false, true);
}
