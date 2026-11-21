import { createSignal, Show } from 'solid-js';
import { icons } from '../../design/icons';
import css from './Picture.module.css';

type PictureProps = {
  id?: string;
  src?: string;
  class?: string;
};

type State = 'loading' | 'loaded' | 'failed';

function Loaded(props: { id?: string; src?: string }) {
  const [state, setState] = createSignal<State>(
    props.src || InventoryAPI.GetItemInventoryImage(props.id ?? '') ? 'loading' : 'failed',
  );
  const loaded = () => setState('loaded');
  const failed = () => setState('failed');
  return (
    <>
      {props.src ? (
        <Image
          class={css.image}
          classList={{ [css.loaded]: state() === 'loaded' }}
          scaling="stretch-to-fit-preserve-aspect"
          src={props.src}
          onImageLoaded={loaded}
          onImageFailedLoad={failed}
        />
      ) : (
        <GenericPanel
          type="ItemImage"
          class={css.image}
          classList={{ [css.loaded]: state() === 'loaded' }}
          scaling="stretch-to-fit-preserve-aspect"
          itemid={props.id}
          onImageLoaded={loaded}
          onImageFailedLoad={failed}
        />
      )}
      <Show when={state() === 'failed'}>
        <Image class={css.missing} src={icons.missing} />
      </Show>
    </>
  );
}

export function Picture(props: PictureProps) {
  return (
    <Panel class={`${css.picture} ${props.class ?? ''}`} hittest={false}>
      <Show when={props.src || props.id} keyed>
        {() => <Loaded id={props.id} src={props.src} />}
      </Show>
    </Panel>
  );
}
