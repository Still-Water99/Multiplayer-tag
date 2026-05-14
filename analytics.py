import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import time

def compute_stats(df,player_index):
    df=df[df['player_index']==player_index].copy()
    df=df.sort_values('timestamp')

    df['packets_lost']=df['packets_sent']-df['packets_received']
    df['loss_percent']=df['packets_lost']/df['packets_sent']*100
    return df

def plot_stats():
    df=pd.read_csv('stats.csv')
    df.columns=['timestamp','player_index','packets_received','packets_sent','x','y','is_it']

    players=df['player_index'].unique()
    fig, axes = plt.subplots(2, 2, figsize=(12, 8))
    fig.suptitle('Multiplayer Tag - Network Stats')
    
    for player in players:
        pdata = compute_stats(df, player)
        t = (pdata['timestamp'] - pdata['timestamp'].iloc[0]) / 1000  # seconds
        
        # packet loss over time
        axes[0][0].plot(t, pdata['loss_percent'], label=f'Player {player}')
        axes[0][0].set_title('Packet Loss %')
        axes[0][0].set_xlabel('Time (s)')
        axes[0][0].set_ylabel('Loss %')
        axes[0][0].legend()
        
        # packets received over time
        axes[0][1].plot(t, pdata['packets_received'], label=f'Player {player}')
        axes[0][1].set_title('Packets Received (cumulative)')
        axes[0][1].set_xlabel('Time (s)')
        axes[0][1].set_ylabel('Count')
        axes[0][1].legend()
        
        # player path (x,y over time)
        axes[1][0].plot(pdata['x'], pdata['y'], label=f'Player {player}', alpha=0.6)
        axes[1][0].set_title('Player Paths')
        axes[1][0].set_xlabel('X')
        axes[1][0].set_ylabel('Y')
        axes[1][0].legend()
    
    # who was it over time
    it_data = df[df['is_it'] == True]
    axes[1][1].scatter(
        (it_data['timestamp'] - df['timestamp'].min()) / 1000,
        it_data['player_index'],
        s=5
    )
    axes[1][1].set_title('"It" over time')
    axes[1][1].set_xlabel('Time (s)')
    axes[1][1].set_ylabel('Player index')
    axes[1][1].set_yticks(list(players))
    
    plt.tight_layout()
    plt.savefig('stats.png')
    plt.show()

plot_stats()