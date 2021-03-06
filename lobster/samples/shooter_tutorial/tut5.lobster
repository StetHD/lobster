include "std.lobster"
include "vec.lobster"
include "color.lobster"

fatal(gl_window("Shooter Tutorial", 640, 480))

worldsize :== 20.0

playerpos := xy_0
playerspeed :== 10

struct bullet { pos:xy_f, dir:xy_f }

firerate :== 0.1
bulletspeed :== 15
bullets := []
lastbullet := gl_time()

struct enemy { pos:xy_f, hp:int }

enemyrate := 1.0
enemyspeed :== 3
enemymaxhp :== 5
enemies := []
lastenemy := gl_time()

def renderpointytriangle(pos, dir):
    gl_translate pos:
        gl_rotate_z dir:
            gl_polygon([ [ -0.5, 0.5 ], xy_x, [ -0.5, -0.5 ] ])

while gl_frame() and gl_button("escape") != 1:
    gl_clear(color_black)
    gl_color(color_white)

    gl_translate(float(gl_windowsize()) / 2.0)
    scale := gl_windowsize().y / worldsize
    gl_scale(scale)

    dir := xy_f { (gl_button("d") >= 1) - (gl_button("a") >= 1),
                  (gl_button("s") >= 1) - (gl_button("w") >= 1) }
    newpos := playerpos + normalize(dir) * gl_deltatime() * playerspeed
    if !any(abs(newpos) > float(gl_windowsize()) / scale / 2):
        playerpos = newpos

    tomouse := normalize(gl_localmousepos(0) - playerpos)

    if lastenemy < gl_time():
        enemies.push(enemy { sincos(rnd(360)) * worldsize * 2, enemymaxhp })
        lastenemy += enemyrate
        enemyrate *= 0.999

    if lastbullet < gl_time():
        bullets.push(bullet { playerpos, tomouse })
        lastbullet += firerate

    for(enemies) e:
        playerdir := normalize(playerpos - e.pos)
        e.pos += playerdir * gl_deltatime() * enemyspeed
        for(bullets) b:
            if magnitude(b.pos - e.pos) < 1:
                e.hp = max(e.hp - 1, 0)
                b.pos = xy_x * worldsize * 10
        gl_color lerp(color_red, color_blue, div(e.hp, enemymaxhp)):
            renderpointytriangle(e.pos, playerdir)

    enemies = filter(enemies): _.hp

    for(bullets) b:
        b.pos += b.dir * gl_deltatime() * bulletspeed
        gl_translate b.pos:
            gl_color color_yellow:
                gl_circle(0.2, 20)

    bullets = filter(bullets) b:
        magnitude(b.pos) < worldsize * 2

    gl_translate gl_localmousepos(0):
        gl_linemode 1:
            gl_color color_grey:
                gl_circle(0.5, 20)

    renderpointytriangle(playerpos, tomouse)
