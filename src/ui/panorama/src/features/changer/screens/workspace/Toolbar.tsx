import { For } from 'solid-js';
import { anchorId } from '../../../../components/Menu/menu';
import { iconPath } from '../../../../design/icons';
import type { Entry } from '../../catalog';
import * as game from '../../game';
import { anthem, cheer, voice, zoom, zoomed } from '../../scene';
import css from './Toolbar.module.css';

type Tool = { icon: string; token: string; fallback: string; active?: () => boolean; press: (id: string) => void };

function ToolButton(props: { tool: Tool }) {
  const id = anchorId();
  return (
    <Button
      id={id}
      class={css.tool}
      classList={{ [css.active]: props.tool.active?.() === true }}
      tabindex="auto"
      selectionpos="auto"
      tooltip={game.localized(props.tool.token, props.tool.fallback)}
      onactivate={() => props.tool.press(id)}
    >
      <Image class={css.icon} src={iconPath(props.tool.icon)} />
    </Button>
  );
}

function toolsOf(entry: Entry): Tool[] {
  if (entry.kind === 'music') {
    return [{ icon: 'ui/unmuted', token: '#tooltip_inspect_play_mvp_anthem', fallback: 'MVP', press: anthem }];
  }
  if (entry.kind !== 'agent') return [];
  return [
    { icon: 'ui/cheer', token: '#tooltip_character_cheer', fallback: 'Cheer', press: () => cheer(entry, false) },
    { icon: 'ui/defeat_anim', token: '#tooltip_character_defeat', fallback: 'Defeat', press: () => cheer(entry, true) },
    { icon: 'ui/sound_off', token: '#tooltip_character_voice', fallback: 'Voice', press: voice },
    {
      icon: zoomed() ? 'ui/zoom_out' : 'ui/zoom_in',
      token: '#tooltip_character_zoom',
      fallback: 'Zoom',
      active: zoomed,
      press: zoom,
    },
  ];
}

export function Toolbar(props: { entry: Entry }) {
  return (
    <Panel class={css.toolbar}>
      <For each={toolsOf(props.entry)}>{(tool) => <ToolButton tool={tool} />}</For>
    </Panel>
  );
}
