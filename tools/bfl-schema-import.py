#!/usr/bin/env python3
"""Import pinned BFL contracts without network access; emit an apply_patch patch.

Copyright 2026 Qore Technologies, s.r.o.; MIT license (see COPYING.MIT).
Vendor inputs retain their original attribution; see bfl-provenance.json.
"""
import ast
import copy
import difflib
import hashlib
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
DEST = ROOT / 'qlib/BflRestClient'
SOURCES = {
    'main': ('https://api.bfl.ai/openapi.json', '63d11365e56b138a56f3918ba528a7b0240c8957f48d3c0d55ad908b78969ce1', 84971),
    'licenses': ('https://api.bfl.ai/v1/licenses/docs/openapi.json', '758553faad00cd9bf697532f82d1ed7d95a37f8f04e3a475d80a74455790012b', 1741),
    'video-edit': ('https://docs.bfl.ai/openapi/video-edit.json', '92008ad79d7ced2722be954ed66c437c57746beb9df9821927203320364e413c', 3650),
    'comfy': ('https://raw.githubusercontent.com/black-forest-labs/bfl-comfy-nodes/c9a6011c2452ce103e0676329617f5f0ad6266a4/bfl_api.py', '837f9d8e8d0eb4928ebe4776544d7ebf53ab058c088489b1fb0b91fa18b9e0dd', 21143),
    'lora': ('https://docs.bfl.ai/flux_2/flux2_lora_inference.md', '8e5b1069719fa43a79cb423109290b21cf741572289b2e471ab1e2b80b1da3d6', 12757),
    'comfy-license': ('https://raw.githubusercontent.com/black-forest-labs/bfl-comfy-nodes/c9a6011c2452ce103e0676329617f5f0ad6266a4/LICENSE', '514e3da4e34aeb127898a436d94a054ace1470b518e018cf26a870fbf978fe74', 1074),
}


def load(name):
    suffix = 'txt' if name in ('comfy', 'comfy-license', 'lora') else 'json'
    raw = (DEST / f'bfl-vendor-{name}.{suffix}').read_bytes()
    # apply_patch stores text with a final newline. Only this packaging difference is permitted.
    original = raw[:SOURCES[name][2]]
    if raw[len(original):].strip(b'\n') or hashlib.sha256(original).hexdigest() != SOURCES[name][1]:
        raise ValueError(f'{name}: vendor revision changed; review it before updating the importer')
    return original.decode() if suffix == 'txt' else json.loads(original)


def ref(name):
    return {'$ref': '#/components/schemas/' + name}


def normalize():
    load('comfy-license')
    main, licenses, video = (load(n) for n in ('main', 'licenses', 'video-edit'))
    result = copy.deepcopy(main)
    schemas = result['components']['schemas']
    repairs = []
    for name in ('Flux2Inputs', 'Flux2KleinInputs', 'Flux2FlexInputs'):
        for field in ('width', 'height'):
            node = schemas[name]['properties'][field]
            assert node['default'] == 0 and node['anyOf'][0]['minimum'] == 64
            del node['default']
    repairs.append('Remove invalid zero defaults from nullable FLUX.2 dimensions; preserve minimum 64 and absence/null.')
    # Licensing is a separate official API on the same origin. Namespace all local references.
    def namespace(value):
        if isinstance(value, dict):
            return {k: ('#/components/schemas/License_' + v.split('/')[-1]
                        if k == '$ref' else namespace(v)) for k, v in value.items()}
        if isinstance(value, list):
            return [namespace(v) for v in value]
        return value
    result['paths'].update(namespace(licenses['paths']))
    schemas.update({'License_' + k: namespace(v) for k, v in licenses['components']['schemas'].items()})
    usage = result['paths']['/v1/licenses/models/{model_slug}/usage']['post']
    usage['responses']['200']['content']['application/json']['schema'] = {
        'type': 'object', 'properties': {'message': {'type': 'string', 'description': 'Usage reporting confirmation.'}},
        'description': 'Confirmation of a billing-affecting model usage report.', 'additionalProperties': True}
    repairs.append('Describe the licensing confirmation message from the official 200 example; retain extension fields.')
    # Keep the primary video-edit contract, including its optional user field; the companion is a duplicate.
    assert '/v1/flux-tools/video-edit-v1' in video['paths']
    # The current guide still links FLUX Pro. Derive the companion request only from the pinned official SDK.
    sdk = ast.parse(load('comfy'))
    cls = next(n for n in sdk.body if isinstance(n, ast.ClassDef) and n.name == 'FluxPro')
    spec = ast.literal_eval(next(n.value for n in cls.body if isinstance(n, ast.Assign)
        and n.targets[0].id == 'INPUT_SPEC'))
    props = {}
    for key, (kind, *options) in {**spec['required'], **spec['optional']}.items():
        if key == 'api_key_override':
            continue
        opts = options[0] if options else {}
        node = {'type': {'STRING': 'string', 'INT': 'integer', 'FLOAT': 'number',
                         'BOOLEAN': 'boolean', 'IMAGE': 'string'}[kind]}
        node.update({b: opts[a] for a, b in [('default', 'default'), ('min', 'minimum'), ('max', 'maximum')]
                     if a in opts})
        if key in ('width', 'height'):
            node['multipleOf'] = 32
        if key == 'image_prompt':
            node['description'] = 'Base64 image used as a visual reference.'
        props[key] = node
    schemas['FluxProCompanionInputs'] = {'type': 'object', 'properties': props,
        'required': list(spec['required']), 'additionalProperties': False}
    response = copy.deepcopy(result['paths']['/v1/flux-pro-1.1']['post']['responses'])
    result['paths']['/v1/flux-pro'] = {'post': {'summary': 'Start FLUX Pro Image',
        'description': 'Compatibility endpoint documented in the image guide and official SDK. Availability may vary by account.',
        'requestBody': {'required': True, 'content': {'application/json': {'schema': ref('FluxProCompanionInputs')}}},
        'responses': response}}
    # The LoRA guide explicitly specifies base fields plus these two additions.
    lora = load('lora')
    for variant in ('4b', '9b', '9b-kv', '9b-kv-bf16', 'base-4b', 'base-9b'):
        path = '/v1/flux-2-klein-' + variant + '-finetuned'
        assert path in lora and 'The request schema matches the underlying base endpoint' in lora
        body = copy.deepcopy(schemas['Flux2KleinInputs'])
        body['title'] = 'FLUX.2 Klein ' + variant + ' LoRA Inputs'
        body['properties']['finetune_id'] = {'type': 'string', 'minLength': 1,
            'description': 'Owned LoRA name or owner organization ID followed by / and the shared LoRA name.'}
        body['properties']['finetune_strength'] = {'type': 'number', 'default': 1.0,
            'description': 'Strength of the selected LoRA. Include its trigger phrase in the prompt when required.'}
        body['required'].append('finetune_id')
        if variant.startswith('base-'):
            # The public base schema is absent. Expose only common parameters explicitly named in the guide.
            allowed = {'prompt', 'width', 'height', 'seed', 'output_format', 'finetune_id', 'finetune_strength'}
            body['properties'] = {k: v for k, v in body['properties'].items() if k in allowed or k.startswith('input_image')}
        result['paths'][path] = {'post': {'summary': 'Start FLUX.2 Klein ' + variant + ' LoRA Image',
            'description': 'Public beta. Select a LoRA matching this base model and precision. Caller pays for inference. '
                + ('Only common fields documented by the LoRA guide are exposed for this base variant.'
                   if variant.startswith('base-') else 'Availability and pricing may change during beta.'),
            'requestBody': {'required': True, 'content': {'application/json': {'schema': body}}}, 'responses': response}}
    result['info']['title'] = 'Black Forest Labs reviewed REST contract'
    result['info']['version'] = '2026-09-19.1'
    result['servers'] = [{'url': 'https://api.bfl.ai'}]
    result['x-qore-import'] = {'version': 1, 'repairs': repairs,
        'duplicates': ['POST /v1/flux-tools/video-edit-v1 (dedicated video-edit schema)'],
        'supplements': ['FLUX Pro from official SDK', 'Six FLUX.2 Klein LoRA public beta endpoints from official guide'],
        'limits': ['Public base Klein schemas are absent; base LoRA variants expose the explicitly documented common fields.',
            'Legacy finetune operations remain in the current schema despite the October 2025 retirement notice. Availability is unverified.',
            'FLUX Pro remains documented but live generation has not been verified.']}
    return result


def provenance(contract):
    return {'attribution': 'API schemas and descriptions: Black Forest Labs. No schema-specific license is declared. '
                'The official Comfy SDK is MIT licensed; its original 2024 black-forest-labs notice is installed in bfl-vendor-comfy-license.txt.',
        'terms': 'https://bfl.ai/legal/terms-of-service', 'retrieved': '2026-09-19', 'normalization_version': 1,
        'packaging': 'Vendor text is unchanged except an optional final newline added by apply_patch.',
        'sources': {name: {'url': v[0], 'sha256': v[1], 'bytes': v[2]} for name, v in SOURCES.items()},
        'lora': {'url': 'https://docs.bfl.ai/flux_2/flux2_lora_inference.md',
            'packaged_sha256': hashlib.sha256((DEST / 'bfl-vendor-lora.txt').read_bytes()).hexdigest()},
        'operations': [{'method': m.upper(), 'path': p} for p, methods in contract['paths'].items()
                       for m in methods if m in ('get', 'post')], 'review': contract['x-qore-import']}


def main():
    contract = normalize()
    print('*** Begin Patch')
    for name, value in [('bfl-openapi.json', contract), ('bfl-provenance.json', provenance(contract))]:
        path = DEST / name
        output = json.dumps(value, indent=2, ensure_ascii=False) + '\n'
        relative = path.relative_to(ROOT)
        if path.exists():
            diff = list(difflib.unified_diff(path.read_text().splitlines(), output.splitlines()))
            if not diff:
                continue
            print('*** Update File: ' + str(relative))
            for line in diff[2:]:
                print('@@' if line.startswith('@@') else line)
        else:
            print('*** Add File: ' + str(relative))
            for line in output.splitlines():
                print('+' + line)
    print('*** End Patch')


if __name__ == '__main__':
    main()
