#ifndef COLLISION_H
#define COLLISION_H


static bool CheckCollisionPointAndRec(float x, float y, float recx, float recy, float recw, float rech) {
	return x > recx && x < recx + recw && y > recy && y < recy + rech;
}


#endif
