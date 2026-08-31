"""D3D12/SM6 GPU readback of actual lab materials; creates no content assets.

Run with UnrealEditor-Cmd -run=PythonScript -AllowCommandletRendering -d3d12 -sm6.
Checks settled hard/soft equivalence, partial-coverage values, raw safety gating,
and the short accumulator. A Raw-squared implementation fails this test.
"""
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
render = unreal.RenderingLibrary
surface = unreal.load_asset('/Game/Darkwell/Vision/PropLab/M_PropLabSurface')
soft_material = unreal.load_asset('/Game/Darkwell/Vision/PropLab/M_PropLabSoft')
assert surface.get_editor_property('blend_mode') == unreal.BlendMode.BLEND_OPAQUE
surface_mid = unreal.MaterialLibrary.create_dynamic_material_instance(world, surface)
soft_mid = unreal.MaterialLibrary.create_dynamic_material_instance(world, soft_material)
targets = [render.create_render_target2d(world, 16, 16, unreal.TextureRenderTargetFormat.RTF_RGBA16F) for _ in range(4)]
raw, previous, soft_out, output = targets
surface_mid.set_texture_parameter_value('DarkwellLiveCoverageTexture', raw)
surface_mid.set_texture_parameter_value('LabSoftCoverageTexture', previous)
surface_mid.set_scalar_parameter_value('DiagnosticRawCoverageView', 1)
surface_mid.set_vector_parameter_value('FogWorldInvExtent', unreal.LinearColor(0,0,0,0))
soft_mid.set_texture_parameter_value('Raw', raw)
soft_mid.set_texture_parameter_value('Previous', previous)
checks = 0

def clear(target, value):
    render.clear_render_target2d(world, target, unreal.LinearColor(value,value,value,1))

def read(material, target):
    render.draw_material_to_render_target(world, target, material)
    return render.read_render_target_raw_pixel(world, target, 8, 8, False).r

def check(name, actual, expected):
    global checks
    unreal.log(f'LAB_GPU {name} actual={actual:.6f} expected={expected:.6f}')
    assert abs(actual-expected) < .007, f'{name}: {actual} != {expected}'
    checks += 1

for coverage in (0, .1, .25, .5, .75, 1):
    clear(raw, coverage)
    clear(previous, coverage)
    for mode in (0,1,2):
        surface_mid.set_scalar_parameter_value('LabWholeObject', int(mode==0))
        surface_mid.set_scalar_parameter_value('LabSoft', int(mode==2))
        check(f'settled_mode{mode}_raw{coverage}', read(surface_mid, output), 1 if mode==0 else coverage)

surface_mid.set_scalar_parameter_value('LabWholeObject', 0)
surface_mid.set_scalar_parameter_value('LabSoft', 1)
clear(raw, .75)
clear(previous, .25)
check('short_ramp_limits_current_surface', read(surface_mid, output), .25)
clear(raw, 0)
clear(previous, 1)
check('raw_loss_clears_surface_immediately', read(surface_mid, output), 0)
soft_mid.set_scalar_parameter_value('Step', 1/12)
check('raw_loss_clears_accumulator_immediately', read(soft_mid, soft_out), 0)
clear(raw, 1)
clear(previous, 0)
for step in range(1, 13):
    value = read(soft_mid, soft_out)
    check(f'one_of_twelve_ramp_step_{step}', value, step/12)
    # Preserve the GPU result in the next input; no CPU-derived expected value is fed back.
    previous, soft_out = soft_out, previous
    soft_mid.set_texture_parameter_value('Previous', previous)

for target in targets:
    render.release_render_target2d(target)
unreal.log(f'LAB_GPU_MATERIAL_PASS checks={checks} opaque=1 settled_hard_equals_soft=1')
