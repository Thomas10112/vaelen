# VAELEN - raise the atlas from the editor's Python console.
#
# Run it from the editor's Cmd box:
#
#     py "<project>/Tools/EditorAtlas.py"
#
# It does what a person would otherwise do by hand in the Details panel and the
# Place Actors panel: makes sure the level has light, finds or spawns the atlas
# actor, sets the proportions that make a relief legible, builds AELVOR, frames
# the camera on it and prints the report.
#
# Every step is guarded. Unreal's Python bindings rename properties (BuildAelvor
# becomes build_aelvor, bShowTowns becomes show_towns) and the exact spelling has
# never been verified on this project, so a step that fails says so and the rest
# still runs.
#
# STATUS: UNVERIFIED - editor-side, written blind from a Linux container.
import unreal

# ---------------------------------------------------------------- what to build
WORLD_SIZE = 256   # tiles a side; 13.09's gate is at 256
YEARS = 120        # years of history after the pre-history
TILE_SIZE = 400.0  # centimetres a tile - 100 made AELVOR 128 m across
RELIEF = 4000.0    # centimetres of relief per unit of elevation
SLAB = 100.0       # thickness of a tile's slab
SUN_PITCH = -18.0  # a low sun: grazing shadow is what makes a relief readable


def log(message):
    unreal.log("[EditorAtlas] " + message)


def warn(message):
    unreal.log_warning("[EditorAtlas] " + message)


def actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def find_one(cls):
    for actor in actors().get_all_level_actors():
        if isinstance(actor, cls):
            return actor
    return None


def ensure(cls, name, location=None, rotation=None):
    """Return the level's actor of this class, spawning one if there is none."""
    found = find_one(cls)
    if found is not None:
        log("found existing " + name)
        return found
    spawned = actors().spawn_actor_from_class(
        cls,
        location if location is not None else unreal.Vector(0.0, 0.0, 0.0),
        rotation if rotation is not None else unreal.Rotator(0.0, 0.0, 0.0),
    )
    log("spawned " + name)
    return spawned


def set_property(actor, name, value):
    """Set one editor property, saying so when the name is not the right one."""
    try:
        actor.set_editor_property(name, value)
        return True
    except Exception as error:
        warn("could not set '%s': %s" % (name, error))
        return False


# ------------------------------------------------------------------------ light
# A level with no DirectionalLight renders black in Lit mode, which is what makes
# the atlas look like it failed when it did not.
sun = ensure(
    unreal.DirectionalLight, "DirectionalLight",
    unreal.Vector(0.0, 0.0, 10000.0), unreal.Rotator(0.0, SUN_PITCH, 45.0),
)
sun.set_actor_rotation(unreal.Rotator(0.0, SUN_PITCH, 45.0), False)
ensure(unreal.SkyLight, "SkyLight", unreal.Vector(0.0, 0.0, 10000.0))
ensure(unreal.SkyAtmosphere, "SkyAtmosphere")

# ------------------------------------------------------------------------ atlas
atlas = ensure(unreal.VaelenAtlasActor, "VaelenAtlasActor")

set_property(atlas, "world_size", WORLD_SIZE)
set_property(atlas, "years", YEARS)
set_property(atlas, "tile_size", TILE_SIZE)
set_property(atlas, "relief_scale", RELIEF)
set_property(atlas, "slab_height", SLAB)

log("building AELVOR at %d, this blocks the editor for a while" % WORLD_SIZE)
try:
    atlas.build_aelvor()
except Exception as error:
    warn("build_aelvor() failed: %s" % error)
    raise

# ----------------------------------------------------------------------- report
try:
    log("REPORT: " + str(atlas.get_editor_property("report")))
except Exception as error:
    warn("could not read 'report': %s" % error)

# ----------------------------------------------------------------------- camera
# Frame the whole plate from one corner, looking down at it.
span = WORLD_SIZE * TILE_SIZE
try:
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
        unreal.Vector(-span * 0.6, -span * 0.6, span * 0.7),
        unreal.Rotator(0.0, -35.0, 45.0),
    )
    log("camera framed on a plate %.0f m across" % (span / 100.0))
except Exception as error:
    warn("could not move the camera: %s" % error)

log("done - if the viewport is black, the view mode is Lit and something ate the light;")
log("      switch to Unlit to check the colours, then back.")
