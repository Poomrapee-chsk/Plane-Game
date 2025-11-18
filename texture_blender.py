import cv2
import numpy as np


grass_img = cv2.imread(r"resources\textures\pbr\terrain\grass.jpg")
rock_img = cv2.imread(r"resources\textures\pbr\terrain\rock.jpg")
sand_img = cv2.imread(r"resources\textures\pbr\terrain\sand.jpg")
snow_img = cv2.imread(r"resources\textures\pbr\terrain\snow.jpg")
water_img = cv2.imread(r"resources\textures\pbr\terrain\water.jpg")

height_map = cv2.imread(r"resources\terrain\heightmaps\iceland_heightmap.png")
texture_img = np.empty_like(height_map)
height_map = (cv2.cvtColor(height_map,cv2.COLOR_BGR2GRAY) - 32) / 64


def smooth_blend(a,b,min_h,max_h,h):
    ratio = np.clip((h - min_h) / (max_h - min_h),0,1)
    return (a * (1 - ratio)) + (b * ratio)

grass_img = np.tile(grass_img,(10,10,1))
rock_img = np.tile(rock_img,(10,10,1))
sand_img = np.tile(sand_img,(20,20,1))
snow_img = np.tile(snow_img,(10,10,1))
water_img = np.tile(water_img,(10,10,1))

cv2.imwrite(r"resources\textures\pbr\terrain\grass.jpg",grass_img)
cv2.imwrite(r"resources\textures\pbr\terrain\rock.jpg",rock_img)
cv2.imwrite(r"resources\textures\pbr\terrain\sand.jpg",sand_img)
cv2.imwrite(r"resources\textures\pbr\terrain\snow.jpg",snow_img)
cv2.imwrite(r"resources\textures\pbr\terrain\water.jpg",water_img)



print(texture_img.shape)
print(height_map.shape)
print(water_img.shape)

# for x in range(texture_img.shape[0]):
#     for y in range(texture_img.shape[1]):
#         if (height_map[x][y] < 0.4):
#             texture_img[x,y] = smooth_blend(water_img[x,y,:],sand_img[x,y,:],0.0,0.3,height_map[x,y])
#         elif (height_map[x][y] < 0.7):
#             texture_img[x,y] = smooth_blend(sand_img[x,y,:],grass_img[x,y,:],0.3,0.5,height_map[x,y])
#         elif (height_map[x][y] < 0.85):
#             texture_img[x,y] = smooth_blend(grass_img[x,y,:],rock_img[x,y,:],0.5,0.75,height_map[x,y])
#         else:
#             texture_img[x,y] = smooth_blend(rock_img[x,y,:],snow_img[x,y,:],0.75,1.0,height_map[x,y])

# cv2.imwrite("iceland_texture.jpg",texture_img)
