package dev.skycraft.classic;
import net.fabricmc.api.ModInitializer;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerTickEvents;
import dev.skycraft.link.SkyLink;
import dev.skycraft.SkyCraft;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.entity.projectile.arrow.AbstractArrow;
import net.fabricmc.loader.api.FabricLoader;
import net.minecraft.world.item.Items;
import net.minecraft.world.item.ItemStack;
import java.nio.file.Files;
public final class ClassicGameplay implements ModInitializer {
    public void onInitialize() {
        ServerTickEvents.END_SERVER_TICK.register(server -> {
            if (!SkyLink.active()) return;
            for (var player:server.getPlayerList().getPlayers()) {
                if (!player.isAlive() || player.isSpectator()) continue;
                var request=FabricLoader.getInstance().getConfigDir().resolve(ClassicRules.LostBow.request);
                if(Files.isRegularFile(request)) {
                    boolean hasBow=false;
                    for(int slot=0;slot<player.getInventory().getContainerSize();slot++)
                        if(player.getInventory().getItem(slot).is(Items.BOW))hasBow=true;
                    if(!hasBow) {
                        var bow=new ItemStack(Items.BOW);
                        if(player.getInventory().getItem(ClassicRules.LostBow.slot).isEmpty())player.getInventory().setItem(ClassicRules.LostBow.slot,bow);
                        else if(!player.getInventory().add(bow))continue;
                        player.containerMenu.broadcastChanges();
                        SkyCraft.LOG.info("Classic recovery: replaced the creator's lost bow once");
                    }
                    try {Files.delete(request);}catch(java.io.IOException error){SkyCraft.LOG.warn("Classic recovery request could not be consumed",error);}
                }
                for (Entity e:player.level().getEntities(player,player.getBoundingBox().inflate(ClassicRules.Horizontal.value,ClassicRules.Vertical.value,ClassicRules.Horizontal.value))) {
                    if(e.isRemoved() || !(e instanceof ItemEntity || e instanceof AbstractArrow))continue;
                    int before=e instanceof ItemEntity item?item.getItem().getCount():1;
                    e.playerTouch(player);
                    int after=e instanceof ItemEntity item?item.getItem().getCount():e.isRemoved()?0:1;
                    if(e.isRemoved() || after<before)SkyCraft.LOG.info("Classic pickup: actual item collected, count {} -> {}",before,after);
                }
            }
        });
    }
}
