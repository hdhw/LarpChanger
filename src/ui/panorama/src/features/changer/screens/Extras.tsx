import { createSignal, Show } from 'solid-js';
import type { TagMode } from '../../../../generated/api';
import { ActionButton } from '../../../components/ActionButton/ActionButton';
import { DropButton } from '../../../components/DropButton/DropButton';
import { FieldLabel } from '../../../components/FieldLabel/FieldLabel';
import { Picture } from '../../../components/Picture/Picture';
import { Segmented } from '../../../components/Segmented/Segmented';
import { SliderField } from '../../../components/SliderField/SliderField';
import { TextField } from '../../../components/TextField/TextField';
import { t } from '../../../i18n';
import { kitName } from '../mvp';
import css from './Extras.module.css';

const api = OpenChangerAPI.extras;

const modes: { value: TagMode; label: string }[] = [
  { value: 'static', label: t.tagStatic },
  { value: 'scroll', label: t.tagScroll },
  { value: 'type', label: t.tagType },
  { value: 'blink', label: t.tagBlink },
];

type RangeProps = { value: number; min: number; onChange: (value: number) => void; onRelease: (value: number) => void };

function Range(props: RangeProps) {
  return (
    <Panel class={css.ruler}>
      <Panel class={css.track} hittest={false} />
      <SliderField
        value={props.value}
        min={props.min}
        max={100}
        step={1}
        onChange={props.onChange}
        onRelease={props.onRelease}
      />
    </Panel>
  );
}

export function Extras() {
  const saved = api.settings();
  const [packs, setPacks] = createSignal(api.packs());
  const [pack, setPack] = createSignal(packs().includes(saved.Pack) ? saved.Pack : '');
  const [volume, setVolume] = createSignal(saved.Volume);
  const [tag, setTag] = createSignal(saved.Tag);
  const [mode, setMode] = createSignal<TagMode>(saved.Mode);
  const [speed, setSpeed] = createSignal(saved.Speed);
  const [tagOn, setTagOn] = createSignal(saved.TagOn);
  const [songs, setSongs] = createSignal(api.songs());
  const [song, setSong] = createSignal(songs().includes(saved.Mvp) ? saved.Mvp : '');
  const [mvpVolume, setMvpVolume] = createSignal(saved.MvpVolume);
  const [title, setTitle] = createSignal(saved.MvpTitle);
  const [artist, setArtist] = createSignal(saved.MvpArtist);
  const [cover, setCover] = createSignal(api.mvpIcon());

  const saveSound = () => api.setSound(pack(), volume());
  const saveMvp = () => {
    api.setMvp(song(), mvpVolume(), title(), artist());
    setCover(api.mvpIcon());
  };
  const mvp =
    <T,>(set: (value: T) => void) =>
    (value: T) => {
      set(value);
      saveMvp();
    };
  const saveTag = () => api.setTag(tag(), mode(), speed(), tagOn());
  const sound =
    <T,>(set: (value: T) => void) =>
    (value: T) => {
      set(value);
      saveSound();
    };
  const tagged =
    <T,>(set: (value: T) => void) =>
    (value: T) => {
      set(value);
      saveTag();
    };

  const packItems = () => {
    setPacks(api.packs());
    return [
      { label: t.off, current: pack() === '', on: () => sound(setPack)('') },
      ...packs().map((name) => ({
        label: name,
        current: pack() === name,
        on: () => sound(setPack)(name),
      })),
    ];
  };

  const songItems = () => {
    setSongs(api.songs());
    return [
      { label: t.off, current: song() === '', on: () => mvp(setSong)('') },
      ...songs().map((name) => ({
        label: name,
        current: song() === name,
        on: () => mvp(setSong)(name),
      })),
    ];
  };

  return (
    <Panel class={css.page}>
      <Label class={css.heading} text={t.killSound} />
      <Panel class={css.row}>
        <DropButton fill class={css.grow} value={pack() || t.off} active={pack() !== ''} items={packItems} />
        <ActionButton label={t.preview} small enabled={pack() !== ''} onPress={() => api.previewSound()} />
        <ActionButton label={t.openFolder} variant="ghost" small onPress={() => api.openFolder()} />
      </Panel>
      <FieldLabel text={t.volume} />
      <Range value={volume()} min={0} onChange={setVolume} onRelease={sound(setVolume)} />
      <Label class={css.hint} text={t.killSoundHint} />

      <Label class={css.heading} text={t.mvpSong} />
      <Panel class={css.row}>
        <DropButton fill class={css.grow} value={song() || t.off} active={song() !== ''} items={songItems} />
        <ActionButton label={t.preview} small enabled={song() !== ''} onPress={() => api.previewMvp()} />
        <ActionButton label={t.openFolder} variant="ghost" small onPress={() => api.openMvpFolder()} />
      </Panel>
      <FieldLabel text={t.volume} />
      <Range value={mvpVolume()} min={0} onChange={setMvpVolume} onRelease={mvp(setMvpVolume)} />
      <Show when={song() !== ''}>
        <Panel class={css.kit}>
          <Panel class={css.cover}>
            <Picture src={cover() || undefined} />
          </Panel>
          <Panel class={css.kitFields}>
            <FieldLabel text={t.mvpTitle} />
            <TextField value={title()} maxchars={48} placeholder="Paparazzi" onChange={mvp(setTitle)} />
            <FieldLabel text={t.mvpArtist} />
            <TextField value={artist()} maxchars={48} placeholder="Lady Gaga" onChange={mvp(setArtist)} />
            <Label class={css.hint} text={kitName(title(), artist())} />
          </Panel>
        </Panel>
      </Show>
      <Label class={css.hint} text={t.mvpSongHint} />

      <Label class={css.heading} text={t.clanTag} />
      <Panel class={css.row}>
        <Segmented
          options={[
            { value: 'off', label: t.off },
            { value: 'on', label: t.tagOn },
          ]}
          value={tagOn() ? 'on' : 'off'}
          onChange={(value) => tagged(setTagOn)(value === 'on')}
        />
      </Panel>
      <FieldLabel text={t.tagText} />
      <TextField value={tag()} maxchars={12} placeholder="LARP" onChange={tagged(setTag)} />
      <Panel class={css.row}>
        <Segmented options={modes} value={mode()} onChange={tagged(setMode)} />
      </Panel>
      <Show when={mode() !== 'static'}>
        <FieldLabel text={t.speed} />
        <Range value={speed()} min={1} onChange={setSpeed} onRelease={tagged(setSpeed)} />
      </Show>
    </Panel>
  );
}
